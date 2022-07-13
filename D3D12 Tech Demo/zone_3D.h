// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#ifndef _APP_GFX_CONSTANTS
#define _APP_GFX_CONSTANTS

#ifdef _WIN64
#define APP_GfxLib App_D3D12
#define GfxLib_D3D12
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

/// <summary>provide one of 6 hexagonal corner points</summary>
inline XMFLOAT2 HexCorner(float fSize, uint uI)
{
	float fAngleDeg = 60.f * (float)(uI % 6) - 30.f;
	float fAngleRad = (XM_PI / 180.f) * fAngleDeg;
	return XMFLOAT2{ fSize * cos(fAngleRad), fSize * sin(fAngleRad) };
}

/// <summary>provide vector to neighbour field (index 0..5)</summary>
inline XMFLOAT2 HexNext(float fSize, uint uI)
{
	const float fMinW = sqrt(fSize * fSize - (fSize * .5f * fSize * .5f)) * 2.f;
	float fAngleDeg = 60.f * (float)(uI % 6);
	float fAngleRad = (XM_PI / 180.f) * fAngleDeg;
	return XMFLOAT2{ fMinW * cos(fAngleRad), fMinW * sin(fAngleRad) };
}

/// <summary>Cartesian to hex coordinates</summary>
inline XMFLOAT2 HexUV(float fX, float fY)
{
	// hex coords       (u, v) = (          .5 * x + .5 * y,        y ) 
	// hex coord scaled (u, v) = ((sqrt(3.f) * x + y) / 3.f, y / 1.5f )
	return XMFLOAT2{ (sqrt(3.f) * fX + fY) / 3.f, fY / 1.5f };
}

/// <summary>Hex to cartesian coordinates</summary>
inline XMFLOAT2 HexXY(float fU, float fV)
{
	// get cartesian coords
	return XMFLOAT2{ (fU * 3.f - fV * 1.5f) / sqrt(3.f), fV * 1.5f };
}

/// <summary>Simple Sample vertex</summary>
struct VertexPosCol
{
	XMFLOAT3 sPos;
	XMFLOAT4 sColor;
};

/// <summary>hlsl attribute structure</summary>
struct PosNorm
{
	XMFLOAT3 vPosition;
	XMFLOAT3 vNormal;
};

/// <summary>hlsl attribute structure</summary>
struct RayPayload
{
	XMFLOAT4 vColor;
	XMFLOAT3 vDir;
};

/// <summary>Simple Sample constants</summary>
struct ConstantsScene
{
	/// <summary>world view projection</summary>
	XMFLOAT4X4 sWVP = XMFLOAT4X4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
	/// <summary>time (x - total, y - delta, z - fps total, w - fps)</summary>
	XMFLOAT4 sTime;
	/// <summary>viewport (x - topLeftX, y - topLeftY, z - width, w - height)</summary>
	XMFLOAT4 sViewport;
	/// <summary>mouse (x - x position, y - y position, z - buttons (uint), w - wheel (uint)</summary>
	XMFLOAT4 sMouse;
	/// <summary>hexagonal uv (x - x cartesian center, y - y cartesian center, z - u center, w - v center)</summary>
	XMFLOAT4 sHexUV;
	/// <summary>camera position (xyz - position)</summary>
	XMFLOAT4 sCamPos;
	/// <summary>camera velocity 3d vector (xyz - direction)</summary>
	XMFLOAT4 sCamVelo;
	/// <summary>invers world view projection</summary>
	XMFLOAT4X4 sWVPrInv = XMFLOAT4X4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
	/// hex data (x - number of vertices per hex tile, xyz reserved)
	XMUINT4 sHexData;
};

/// <summary>round up to nearest multiple of 256</summary>
inline unsigned Align8Bit(unsigned uSize) { return (uSize + 255) & ~255; }

inline unsigned Align(unsigned size, unsigned alignment) { return (size + (alignment - 1)) & ~(alignment - 1); }

// Allocate upload heap buffer and fill with input data.
inline signed AllocateUploadBuffer(
	ID3D12Device* pDevice,
	void* pData,
	UINT64 datasize,
	ID3D12Resource** ppResource,
	const wchar_t* resourceName = nullptr)
{
	auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(datasize);
	ThrowIfFailed(pDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(ppResource)));
	if (resourceName)
	{
		(*ppResource)->SetName(resourceName);
	}

	if (pData)
	{
		void* pMappedData;
		ThrowIfFailed((*ppResource)->Map(0, nullptr, &pMappedData));
		memcpy(pMappedData, pData, datasize);
		(*ppResource)->Unmap(0, nullptr);
	}
	return APP_FORWARD;
}

inline signed AllocateUAVBuffer(
	ID3D12Device* pDevice,
	UINT64 bufferSize,
	ID3D12Resource** ppResource,
	D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
	const wchar_t* resourceName = nullptr)
{
	if (bufferSize == 0)
	{
		*ppResource = nullptr;
		return APP_ERROR;
	}

	auto defaultProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ThrowIfFailed(pDevice->CreateCommittedResource(
		&defaultProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		initialResourceState,
		nullptr,
		IID_PPV_ARGS(ppResource)));
	if (resourceName)
	{
		(*ppResource)->SetName(resourceName);
	}

	return APP_FORWARD;
}

/// <summary>resource barrier helper</summary>
struct CD3DX12_RB_TRANSITION : public D3D12_RESOURCE_BARRIER
{
	explicit CD3DX12_RB_TRANSITION(
		ID3D12Resource* psResource,
		D3D12_RESOURCE_STATES eStatePrev,
		D3D12_RESOURCE_STATES eStatePost,
		UINT uSub = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		D3D12_RESOURCE_BARRIER_FLAGS eFlags = D3D12_RESOURCE_BARRIER_FLAG_NONE)
	{
		this->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		this->Flags = eFlags;
		this->Transition.pResource = psResource;
		this->Transition.StateBefore = eStatePrev;
		this->Transition.StateAfter = eStatePost;
		this->Transition.Subresource = uSub;
	}

	/// <summary>execute transit</summary>
	static inline void ResourceBarrier(
		ID3D12GraphicsCommandList* psCmdList,
		ID3D12Resource* psResource,
		D3D12_RESOURCE_STATES eStatePrev,
		D3D12_RESOURCE_STATES eStatePost,
		UINT uSub = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		D3D12_RESOURCE_BARRIER_FLAGS eFlags = D3D12_RESOURCE_BARRIER_FLAG_NONE)
	{
		const D3D12_RESOURCE_BARRIER sDc = {
			D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			eFlags,
			{
				psResource,
				uSub,
				eStatePrev,
				eStatePost
			}
		};
		psCmdList->ResourceBarrier(1, &sDc);
	}
};

/// <summary>sampler types enumeration</summary>
enum struct SamplerTypes : unsigned
{
	MinMagMipPointUVWClamp
};
/// <summary>number of samplers contained in s_asSamplerDcs</summary>
constexpr unsigned s_uSamplerN = 1;

const D3D12_SAMPLER_DESC s_asSamplerDcs[s_uSamplerN] =
{
	// MinMagMipPointUVWClamp
	{
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		0.0F,
		0,
		D3D12_COMPARISON_FUNC_ALWAYS,
		{ 0.0F, 0.0F, 0.0F, 0.0F },
		0.0F,
		D3D12_FLOAT32_MAX
	},
};

// Shader record = {{Shader ID}, {RootArguments}}
class ShaderRecord
{
public:
	ShaderRecord(void* pShaderIdentifier, unsigned int shaderIdentifierSize) :
		shaderIdentifier(pShaderIdentifier, shaderIdentifierSize)
	{
	}

	ShaderRecord(void* pShaderIdentifier, unsigned int shaderIdentifierSize, void* pLocalRootArguments, unsigned int localRootArgumentsSize) :
		shaderIdentifier(pShaderIdentifier, shaderIdentifierSize),
		localRootArguments(pLocalRootArguments, localRootArgumentsSize)
	{
	}

	void CopyTo(void* dest) const
	{
		uint8_t* byteDest = static_cast<uint8_t*>(dest);
		memcpy(byteDest, shaderIdentifier.ptr, shaderIdentifier.size);
		if (localRootArguments.ptr)
		{
			memcpy(byteDest + shaderIdentifier.size, localRootArguments.ptr, localRootArguments.size);
		}
	}

	struct PointerWithSize {
		void* ptr;
		unsigned int size;

		PointerWithSize() : ptr(nullptr), size(0) {}
		PointerWithSize(void* _ptr, unsigned int _size) : ptr(_ptr), size(_size) {};
	};
	PointerWithSize shaderIdentifier;
	PointerWithSize localRootArguments;
};

// Shader table = {{ ShaderRecord 1}, {ShaderRecord 2}, ...}
class ShaderTable
{
	Microsoft::WRL::ComPtr<ID3D12Resource> m_bufferResource;

	uint8_t* m_mappedShaderRecords;
	unsigned int m_shaderRecordSize;

	bool closed;

	// Debug support
	std::wstring m_name;
	std::vector<ShaderRecord> m_shaderRecords;

	ShaderTable() : closed(false) {}
public:
	ShaderTable(ID3D12Device* psDevice, unsigned int numShaderRecords, unsigned int shaderRecordSize, LPCWSTR resourceName = nullptr)
		: m_name(resourceName), closed(false)
	{
		m_shaderRecordSize = Align(shaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
		m_shaderRecords.reserve(numShaderRecords);

		unsigned int bufferSize = numShaderRecords * m_shaderRecordSize;

		auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
		ThrowIfFailed(psDevice->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_bufferResource.GetAddressOf())));

		if (resourceName)
		{
			m_bufferResource->SetName(resourceName);
		}

		// Map the data.
		CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_bufferResource->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedShaderRecords)));
	}

	void Add(const ShaderRecord& shaderRecord)
	{
		if (closed)
			throw std::exception("Cannot add to a closed ShaderTable.");

		assert(m_shaderRecords.size() < m_shaderRecords.capacity());

		m_shaderRecords.push_back(shaderRecord);
		shaderRecord.CopyTo(m_mappedShaderRecords);
		m_mappedShaderRecords += m_shaderRecordSize;
	}

	void Close()
	{
		if (closed)
			throw std::exception("Cannot close an already closed ShaderTable.");

		closed = closed;

		m_bufferResource->Unmap(0, nullptr);
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> GetResource() { return m_bufferResource; };

	unsigned int GetShaderRecordSize() { return m_shaderRecordSize; }

	// Pretty-print the shader records.
	void DebugPrint(std::unordered_map<void*, std::wstring> shaderIdToStringMap)
	{
		std::wstringstream wstr;
		wstr << L"|--------------------------------------------------------------------\n";
		wstr << L"|Shader table - " << m_name.c_str() << L": "
			<< m_shaderRecordSize << L" | "
			<< m_shaderRecords.size() * m_shaderRecordSize << L" bytes\n";

		for (unsigned int i = 0; i < m_shaderRecords.size(); i++)
		{
			wstr << L"| [" << i << L"]: ";
			wstr << shaderIdToStringMap[m_shaderRecords[i].shaderIdentifier.ptr] << L", ";
			wstr << m_shaderRecords[i].shaderIdentifier.size << L" + " << m_shaderRecords[i].localRootArguments.size << L" bytes \n";
		}
		wstr << L"|--------------------------------------------------------------------\n";
		wstr << L"\n";
		OutputDebugStringW(wstr.str().c_str());
	}
};

/// <summary>serialize and create a root signature</summary>
inline signed CreateRootSignature(ID3D12Device* psDevice, D3D12_ROOT_SIGNATURE_DESC& sDc, ComPtr<ID3D12RootSignature>* ppsRSign)
{
	ComPtr<ID3DBlob> psBlob;
	ComPtr<ID3DBlob> psError;
	HRESULT nHr = D3D12SerializeRootSignature(&sDc, D3D_ROOT_SIGNATURE_VERSION_1, &psBlob, &psError);
	if (psError != nullptr) { ::OutputDebugStringA((char*)psError->GetBufferPointer()); }
	ThrowIfFailed(nHr);
	ThrowIfFailed(psDevice->CreateRootSignature(1, psBlob->GetBufferPointer(), psBlob->GetBufferSize(), IID_PPV_ARGS(&(*ppsRSign))));
	return APP_FORWARD;
}

#endif

#endif // _APP_GFX_CONSTANTS
