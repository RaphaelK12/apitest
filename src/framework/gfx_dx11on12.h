#pragma once

#include "gfx_dx11.h"
#include "d3d11on12.h"

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
class GfxApiDirect3D11On12 : public GfxApiDirect3D11
{
public:
	GfxApiDirect3D11On12();
	virtual ~GfxApiDirect3D11On12();

	virtual bool Init(const std::string& _title, int _x, int _y, int _width, int _height) override;
	virtual void Shutdown() override;

	inline virtual const char* GetShortName() const override { return SGetShortName(); }
	inline virtual const char* GetLongName() const override { return SGetLongName(); }

	static const char* SGetShortName() { return "d3d11on12"; }
	static const char* SGetLongName() { return "Direct3D 11On12"; }

	virtual EGfxApi GetApiType() const { return EGfxApi::Direct3D11On12; }
	static EGfxApi GetApiTypeStatic() { return EGfxApi::Direct3D11On12; }

	static ID3D11Device*			m_d3d_device;
	static ID3D11DeviceContext*		m_d3d_context;

	virtual void Clear(Vec4 _clearColor, GLfloat _clearDepth) override;
	virtual void SwapBuffers() override;

protected:
	UINT64		m_curFenceValue[NUM_ACCUMULATED_FRAMES];

	virtual bool CreateSwapChain();

	// Create Depth Buffer
	HRESULT CreateDepthBuffer();
	// Create Render Target
	HRESULT CreateRenderTarget();
};