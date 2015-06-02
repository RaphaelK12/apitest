#pragma once

#include "gfx.h"
#include "gfx_dx12_legacy.h"

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
class GfxApiDirect3D12 : public GfxBaseApi
{
public:
	GfxApiDirect3D12();
	virtual ~GfxApiDirect3D12();

	virtual bool Init(const std::string& _title, int _x, int _y, int _width, int _height) override;
	virtual void Shutdown() override;

	virtual void Activate() override;
	virtual void Deactivate() override;
	virtual void Clear(Vec4 _clearColor, GLfloat _clearDepth) override;
	virtual void SwapBuffers() override;

	virtual EGfxApi GetApiType() const { return EGfxApi::Direct3D12; }

	inline virtual const char* GetShortName() const override { return SGetShortName(); }
	inline virtual const char* GetLongName() const override { return SGetLongName(); }

	static const char* SGetShortName() { return "d3d12"; }
	static const char* SGetLongName() { return "Direct3D 12"; }

protected:
	comptr<IDXGISwapChain>				m_SwapChain;

	comptr<ID3D12GraphicsCommandList>	m_BeginCommandList[NUM_ACCUMULATED_FRAMES];
	comptr<ID3D12GraphicsCommandList>	m_EndCommandList[NUM_ACCUMULATED_FRAMES];
	UINT64								m_curFenceValue[NUM_ACCUMULATED_FRAMES];

	// Create Swap Chain
	bool CreateSwapChain();
	// Create Render Target
	HRESULT CreateRenderTarget();
	// Create Depth Buffer
	HRESULT CreateDepthBuffer();
	// Create Default Command Queue
	HRESULT CreateDefaultCommandQueue();
};

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// DX12 Utilities
extern comptr<ID3D12Device>			g_D3D12Device;

// Helper routine for resource barriers
void AddResourceBarrier(
	ID3D12GraphicsCommandList * pCl,
	ID3D12Resource * pRes,
	D3D12_RESOURCE_STATES usageBefore,
	D3D12_RESOURCE_STATES usageAfter);

// Load texture
ID3D12Resource* NewTextureFromDetails(const TextureDetails& _texDetails);

// Get Render Target handle
D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetHandle();
D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencialHandle();