// D3D12 Tech Demo
// (c) 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#ifndef _APP_GFX_CONSTANTS
#define _APP_GFX_CONSTANTS

#ifdef _WIN32
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

/// <summary>Simple Sample vertex</summary>
struct VertexPosCol
{
	XMFLOAT3 sPos;
	XMFLOAT4 sColor;
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
	/// <summary>mouse (x - x position, y - y position, z - buttons (unsigned), w - wheel (unsigned)</summary>
	XMFLOAT4 sMouse;
};

/// <summary>round up to nearest multiple of 256</summary>
inline unsigned Align8Bit(unsigned uSize) { return (uSize + 255) & ~255; }

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

#endif

#endif // _APP_GFX_CONSTANTS
