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

/// <summary>resource barrier helper</summary>
struct CD3DX12_RB_TRANSITION : public D3D12_RESOURCE_BARRIER
{
	explicit CD3DX12_RB_TRANSITION(
		_In_ ID3D12Resource* pResource,
		D3D12_RESOURCE_STATES stateBefore,
		D3D12_RESOURCE_STATES stateAfter,
		UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE)
	{
		this->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		this->Flags = flags;
		this->Transition.pResource = pResource;
		this->Transition.StateBefore = stateBefore;
		this->Transition.StateAfter = stateAfter;
		this->Transition.Subresource = subresource;
	}
};

#endif

#endif // _APP_GFX_CONSTANTS
