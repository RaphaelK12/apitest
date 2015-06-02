#include "gfx_dx11on12.h"
#include "gfx_dx12_legacy.h"
#include "gfx_dx12.h"

#define		BACK_BUFFER_COUNT NUM_ACCUMULATED_FRAMES

static comptr<ID3D12Device>				g_D3D12Device;
static comptr<ID3D12CommandQueue>		g_CommandQueue;
static comptr<ID3D12DescriptorHeap>		g_HeapRTV;
static comptr<ID3D12DescriptorHeap>		g_HeapDSV;
static comptr<ID3D11On12Device>			g_D3d11on12_dev;
static comptr<ID3D11RenderTargetView>	g_D3D11RenderTargetView[BACK_BUFFER_COUNT];
static comptr<ID3D12Resource>			g_BackBuffer[BACK_BUFFER_COUNT];
static comptr<ID3D12Resource>			g_DepthStencilBuffer[BACK_BUFFER_COUNT];

// Finish fence
static comptr<ID3D12Fence> g_FinishFence;
static UINT64 g_finishFenceValue;

GfxBaseApi *CreateGfxDirect3D11On12() { return new GfxApiDirect3D11On12; }

static HWND GetHwnd(SDL_Window* _wnd);

ID3D11Device* GfxApiDirect3D11On12::m_d3d_device;
ID3D11DeviceContext* GfxApiDirect3D11On12::m_d3d_context;

static int		g_curContext = 0;
static int		g_backBufferIndex = 0;
static int		g_ClientWidth;
static int		g_ClientHeight;

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

	g_ClientWidth = _width;
	g_ClientHeight = _height;

	mWnd = SDL_CreateWindow(_title.c_str(), _x, _y, _width, _height, SDL_WINDOW_HIDDEN);

	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&m_dxgi_factory));
	if (FAILED(hr))
		return false;

#if _DEBUG
	ID3D12Debug* d3d12DebugController{};
	D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&d3d12DebugController);
	d3d12DebugController->EnableDebugLayer();
	d3d12DebugController->Release();
#endif

	// Create D3D12 Device
	hr = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void **)&g_D3D12Device);
	if (FAILED(hr))
		return false;

	D3D12_COMMAND_QUEUE_DESC queue_desc;
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queue_desc.Priority = 0;
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue_desc.NodeMask = 0;
	g_D3D12Device->CreateCommandQueue(&queue_desc, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&g_CommandQueue));

	// Feature levels we support
	D3D_FEATURE_LEVEL feature_levels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
	};

	// Create Device
	hr = D3D11On12CreateDevice(
		g_D3D12Device,
		0,
		feature_levels,
		ARRAYSIZE(feature_levels),
		(IUnknown**)&g_CommandQueue,
		1,
		0,
		&m_real_device,
		&m_real_context,
		&m_d3d_feature_level);

	if (FAILED(hr))
		return false;

	m_d3d_device = m_real_device;
	m_d3d_context = m_real_context;

	hr = m_real_device->QueryInterface(__uuidof(ID3D11On12Device), reinterpret_cast<void**>(&g_D3d11on12_dev));

	if (FAILED(hr))
		return false;

	if (!CreateSwapChain()) {
		return false;
	}

	// Create a fence
	hr = (g_D3D12Device->CreateFence(g_finishFenceValue, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void**>(&g_FinishFence)));
	if (FAILED(hr))
		return false;
	for (int i = 0; i < NUM_ACCUMULATED_FRAMES; ++i)
		m_curFenceValue[i] = 0;

	return true;
}

void GfxApiDirect3D11On12::Shutdown()
{
	SafeRelease(mSwapChain);

	if (m_d3d_context) {
		m_d3d_context->ClearState();
	}

	SafeRelease(mDepthStencilView);
	SafeRelease(m_d3d_context);
	SafeRelease(m_real_device);
	SafeRelease(m_dxgi_factory);

	m_d3d_device = 0;
	m_d3d_context = 0;
	mColorView = 0;

	g_HeapDSV.release();
	g_HeapRTV.release();
	g_D3d11on12_dev.release();
	g_CommandQueue.release();

	for (int i = 0; i < BACK_BUFFER_COUNT; ++i)
	{
		g_BackBuffer[i].release();
		g_D3D11RenderTargetView[i].release();
		g_DepthStencilBuffer[i].release();
	}

	g_D3D12Device.release();

	if (mWnd) {
		SDL_DestroyWindow(mWnd);
		mWnd = nullptr;
	}
}

bool GfxApiDirect3D11On12::CreateSwapChain()
{
	int width = 1;
	int height = 1;
	SDL_GetWindowSize(mWnd, &width, &height);

	// Create Swap Chain
	DXGI_SWAP_CHAIN_DESC swap_chain_desc;
	memset(&swap_chain_desc, 0, sizeof(swap_chain_desc));
	swap_chain_desc.BufferDesc.Width = UINT(g_ClientWidth);
	swap_chain_desc.BufferDesc.Height = UINT(g_ClientHeight);
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferCount = BACK_BUFFER_COUNT;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.OutputWindow = GetHwnd(mWnd);
	swap_chain_desc.Windowed = TRUE;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

	if (FAILED(m_dxgi_factory->CreateSwapChain(g_CommandQueue, &swap_chain_desc, &mSwapChain))) {
		return false;
	}
	
	if (FAILED(CreateRenderTarget())) {
		return false;
	}

	if (FAILED(CreateDepthBuffer())) {
		return false;
	}

	return true;
}

// Create Render Target
HRESULT GfxApiDirect3D11On12::CreateRenderTarget()
{
	// Create descriptor heap for RTV
	D3D12_DESCRIPTOR_HEAP_DESC heapDescRTV = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, BACK_BUFFER_COUNT, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };

	HRESULT hr;
	hr = g_D3D12Device->CreateDescriptorHeap(&heapDescRTV, __uuidof(ID3D12DescriptorHeap), (void **)&g_HeapRTV);
	if (FAILED(hr))
		return hr;

	for (int i = 0; i < BACK_BUFFER_COUNT; ++i)
	{
		hr = mSwapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void **)&g_BackBuffer[i]);
		if (FAILED(hr))
			return hr;
	}

	// create render target view
	D3D12_CPU_DESCRIPTOR_HANDLE handle = g_HeapRTV->GetCPUDescriptorHandleForHeapStart();
	UINT descriptorSize = g_D3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (int i = 0; i < BACK_BUFFER_COUNT; ++i)
	{
		hr = mSwapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void **)&g_BackBuffer[i]);
		g_D3D12Device->CreateRenderTargetView(g_BackBuffer[i], 0, handle);

		ID3D11Texture2D* backbuffer;
		D3D11_RESOURCE_FLAGS flags11 = { D3D11_BIND_RENDER_TARGET };
		hr = g_D3d11on12_dev->CreateWrappedResource(g_BackBuffer[i], &flags11,
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT,
			__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backbuffer));
		if (FAILED(hr))
			return hr;

		// Create a Render target view
		hr = m_d3d_device->CreateRenderTargetView(backbuffer, nullptr, &g_D3D11RenderTargetView[i]);
		backbuffer->Release();

		handle.ptr += descriptorSize;
	}

	return S_OK;
}

void GfxApiDirect3D11On12::Clear(Vec4 _clearColor, GLfloat _clearDepth)
{
	// Check out fence
	const UINT64 lastCompletedFence = g_FinishFence->GetCompletedValue();
	if (m_curFenceValue[g_curContext] > lastCompletedFence)
	{
		HANDLE handleEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		g_FinishFence->SetEventOnCompletion(m_curFenceValue[g_curContext], handleEvent);
		WaitForSingleObject(handleEvent, INFINITE);
		CloseHandle(handleEvent);
	}

	mColorView = g_D3D11RenderTargetView[g_backBufferIndex];

	// setup render target
	m_real_context->OMSetRenderTargets(1, &mColorView, mDepthStencilView);

	// Should go somewhere else. 
	D3D11_VIEWPORT vp;
	vp.Width = static_cast<FLOAT>(GetWidth());
	vp.Height = static_cast<FLOAT>(GetHeight());
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_real_context->RSSetViewports(1, &vp);

	m_real_context->ClearRenderTargetView(mColorView, &_clearColor.x);
	m_real_context->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH, _clearDepth, 0);
}

void GfxApiDirect3D11On12::SwapBuffers()
{
	m_d3d_context->Flush();

	mSwapChain->Present(0, 0);

	// setup fence
	m_curFenceValue[g_curContext] = ++g_finishFenceValue;
	g_CommandQueue->Signal(g_FinishFence, g_finishFenceValue);

	g_curContext = (g_curContext + 1) % NUM_ACCUMULATED_FRAMES;
	g_backBufferIndex = (g_backBufferIndex + 1) % BACK_BUFFER_COUNT;
}

// Create Depth Buffer
HRESULT GfxApiDirect3D11On12::CreateDepthBuffer()
{
	DXGI_SWAP_CHAIN_DESC desc;
	mSwapChain->GetDesc(&desc);

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC tex_desc;
	ZeroMemory(&tex_desc, sizeof(tex_desc));
	tex_desc.Width = desc.BufferDesc.Width;
	tex_desc.Height = desc.BufferDesc.Height;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = 1;
	tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	tex_desc.SampleDesc = desc.SampleDesc;
	tex_desc.Usage = D3D11_USAGE_DEFAULT;
	tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	tex_desc.CPUAccessFlags = 0;
	tex_desc.MiscFlags = 0;

	ID3D11Texture2D* d3d_depth_stencil_tex;
	HRESULT hr = m_real_device->CreateTexture2D(&tex_desc, nullptr, &d3d_depth_stencil_tex);
	if (FAILED(hr))
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
	ZeroMemory(&dsv_desc, sizeof(dsv_desc));
	dsv_desc.Format = tex_desc.Format;
	dsv_desc.ViewDimension =
		tex_desc.SampleDesc.Count > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
	dsv_desc.Texture2D.MipSlice = 0;

	ID3D11DepthStencilView* d3d_depth_stencil_view;
	hr = m_real_device->CreateDepthStencilView(d3d_depth_stencil_tex, &dsv_desc, &mDepthStencilView);
	d3d_depth_stencil_tex->Release();

	if (FAILED(hr))
		return hr;

	return S_OK;
}

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
#include "SDL_syswm.h"

HWND GetHwnd(SDL_Window* _wnd)
{
#ifdef _WIN32
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);

	if (SDL_GetWindowWMInfo(_wnd, &info)) {
		return info.info.win.window;
	}
#endif
	return 0;
}