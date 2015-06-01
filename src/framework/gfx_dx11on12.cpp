#include "gfx_dx11on12.h"

static IDXGIFactory*			g_dxgi_factory;
static D3D_FEATURE_LEVEL		g_d3d_feature_level;

GfxBaseApi *CreateGfxDirect3D11On12() { return new GfxApiDirect3D11On12; }

ID3D11Device* GfxApiDirect3D11On12::m_d3d_device;
ID3D11DeviceContext* GfxApiDirect3D11On12::m_d3d_context;

GfxApiDirect3D11On12::GfxApiDirect3D11On12()
{
}
GfxApiDirect3D11On12::~GfxApiDirect3D11On12()
{
}

bool GfxApiDirect3D11On12::Init(const std::string& _title, int _x, int _y, int _width, int _height)
{
	if (!GfxBaseApi::Init(_title, _x, _y, _width, _height)) {
		return false;
	}

	mWnd = SDL_CreateWindow(_title.c_str(), _x, _y, _width, _height, SDL_WINDOW_HIDDEN);

	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&m_dxgi_factory));
	if (FAILED(hr))
		return false;

	IDXGIAdapter* dxgi_adapter;
	hr = m_dxgi_factory->EnumAdapters(0, &dxgi_adapter);
	if (FAILED(hr))
		return false;

	// Device flags
	UINT create_device_flags = 0;
#ifdef _DEBUG
	//    create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;		// it doesn't work on win32 platform, does it??
#endif

	// Feature levels we support
	D3D_FEATURE_LEVEL feature_levels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
	};

	// Create Device
	hr = D3D11CreateDevice(
		dxgi_adapter,
		D3D_DRIVER_TYPE_UNKNOWN,
		nullptr,
		create_device_flags,
		feature_levels,
		ARRAYSIZE(feature_levels),
		D3D11_SDK_VERSION,
		&m_real_device,
		&m_d3d_feature_level,
		&m_real_context);

	dxgi_adapter->Release();

	m_d3d_device = m_real_device;
	m_d3d_context = m_real_context;

	if (FAILED(hr))
		return false;

	if (!CreateSwapChain()) {
		return false;
	}

	// Set the render target and depth targets.
	m_d3d_context->OMSetRenderTargets(1, &mColorView, mDepthStencilView);

	return true;
}

void GfxApiDirect3D11On12::Shutdown()
{
	SafeRelease(mColorView);
	SafeRelease(mDepthStencilView);
	SafeRelease(mSwapChain);

	if (m_d3d_context) {
		m_d3d_context->ClearState();
	}

	SafeRelease(m_d3d_context);
	SafeRelease(m_real_device);
	SafeRelease(g_dxgi_factory);

	if (mWnd) {
		SDL_DestroyWindow(mWnd);
		mWnd = nullptr;
	}
}