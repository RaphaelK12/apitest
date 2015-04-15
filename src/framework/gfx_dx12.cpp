#include "pch.h"

#include "gfx_dx12.h"

#pragma comment(lib, "d3d12.lib")

// Globals
comptr<IDXGIFactory>				g_dxgi_factory;
comptr<ID3D12Device>				g_D3D12Device;
comptr<ID3D12CommandQueue>			g_CommandQueue;
comptr<ID3D12CommandAllocator>		g_CommandAllocator;
comptr<ID3D12GraphicsCommandList>	g_CommandList;
comptr<ID3D12DescriptorHeap>		g_HeapRTV;
comptr<ID3D12DescriptorHeap>		g_HeapDSV;
comptr<ID3D12Resource>				g_BackBuffer;
comptr<ID3D12Resource>				g_DepthStencilBuffer;

int		g_ClientWidth;
int		g_ClientHeight;

// Finish fence
comptr<ID3D12Fence> g_FinishFence;
UINT64 g_finishFenceValue;
HANDLE g_finishFenceEvent;

static int	g_CurrentBufferId = 0;

GfxBaseApi *CreateGfxDirect3D12() { return new GfxApiDirect3D12; }

static HWND GetHwnd(SDL_Window* _wnd);

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
GfxApiDirect3D12::GfxApiDirect3D12()
{
}

// --------------------------------------------------------------------------------------------------------------------
GfxApiDirect3D12::~GfxApiDirect3D12()
{
}

// --------------------------------------------------------------------------------------------------------------------
bool GfxApiDirect3D12::Init(const std::string& _title, int _x, int _y, int _width, int _height)
{
	if (!GfxBaseApi::Init(_title, _x, _y, _width, _height)) {
		return false;
	}

	g_ClientWidth = _width;
	g_ClientHeight = _height;

	mWnd = SDL_CreateWindow(_title.c_str(), _x, _y, _width, _height, SDL_WINDOW_HIDDEN);

	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&g_dxgi_factory));
	if (FAILED(hr))
		return false;

	// Create D3D12 Device
	hr = D3D12CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, D3D12_CREATE_DEVICE_NONE, D3D_FEATURE_LEVEL_11_0, D3D12_SDK_VERSION, __uuidof(ID3D12Device), (void **)&g_D3D12Device);
	if (FAILED(hr))
		return false;

	// Create Swap Chain
	if (!CreateSwapChain())
		return false;

	// Create a fence
	hr = (g_D3D12Device->CreateFence(g_finishFenceValue, D3D12_FENCE_MISC_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void**>(&g_FinishFence)));
	if (FAILED(hr))
		return false;
	g_finishFenceEvent = CreateEvent(nullptr, FALSE, FALSE, "Finish fence event");

	return true;
}

// --------------------------------------------------------------------------------------------------------------------
void GfxApiDirect3D12::Shutdown()
{
	g_FinishFence.release();

	g_HeapRTV.release();
	g_BackBuffer.release();
	g_HeapDSV.release();
	g_DepthStencilBuffer.release();
	m_SwapChain.release();

	g_CommandAllocator.release();
	g_CommandQueue.release();

	g_D3D12Device.release();
	g_dxgi_factory.release();

	if (mWnd) {
		SDL_DestroyWindow(mWnd);
		mWnd = nullptr;
	}
}

// --------------------------------------------------------------------------------------------------------------------
void GfxApiDirect3D12::Activate()
{
	assert(mWnd);
	SDL_ShowWindow(mWnd);
}

// --------------------------------------------------------------------------------------------------------------------
void GfxApiDirect3D12::Deactivate()
{
	assert(mWnd);
	SDL_HideWindow(mWnd);
}

// --------------------------------------------------------------------------------------------------------------------
void GfxApiDirect3D12::Clear(Vec4 _clearColor, GLfloat _clearDepth)
{
	HRESULT hr = g_CommandAllocator->Reset();
	if (FAILED(hr))
		return;

	// reset command list
	g_CommandList->Reset(g_CommandAllocator, 0);

	//AddResourceBarrier(m_CommandList, g_BackBuffer, D3D12_RESOURCE_USAGE_INITIAL, D3D12_RESOURCE_USAGE_RENDER_TARGET);

	D3D12_RESOURCE_BARRIER_DESC barrierDesc =
	{
		D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		{ {
			g_BackBuffer,
			0,
			D3D12_RESOURCE_USAGE_PRESENT,
			D3D12_RESOURCE_USAGE_RENDER_TARGET,
		} },
	};
	g_CommandList->ResourceBarrier(1, &barrierDesc);

	// Bind render target for drawing
	g_CommandList->ClearRenderTargetView(g_HeapRTV->GetCPUDescriptorHandleForHeapStart(), &_clearColor.x, 0, 0);

	// Clear depth buffer
	g_CommandList->ClearDepthStencilView(g_HeapDSV->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_DEPTH, 1.0f, 0, 0, 0);
}

// --------------------------------------------------------------------------------------------------------------------
void GfxApiDirect3D12::SwapBuffers()
{
	// Create Command List first time invoked
	D3D12_RESOURCE_BARRIER_DESC barrierDesc =
	{
		D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		{ {
			g_BackBuffer,
			0,
			D3D12_RESOURCE_USAGE_RENDER_TARGET,
			D3D12_RESOURCE_USAGE_PRESENT,
		} },
	};
	g_CommandList->ResourceBarrier(1, &barrierDesc);

	// Close the command list
	g_CommandList->Close();

	// execute command list
	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_CommandList);

	// Present
	m_SwapChain->Present(0, 0);
	
	// swap command buffer RTV
	g_CurrentBufferId = (g_CurrentBufferId + 1) % 2;
	m_SwapChain->GetBuffer(g_CurrentBufferId, __uuidof(ID3D12Resource), (void**)&g_BackBuffer);
	g_D3D12Device->CreateRenderTargetView( g_BackBuffer , 0 , g_HeapRTV->GetCPUDescriptorHandleForHeapStart());
}

// --------------------------------------------------------------------------------------------------------------------
bool GfxApiDirect3D12::CreateSwapChain()
{
	// Create Swap Chain
	DXGI_SWAP_CHAIN_DESC swap_chain_desc;
	swap_chain_desc.BufferDesc.Width = UINT(g_ClientWidth);
	swap_chain_desc.BufferDesc.Height = UINT(g_ClientHeight);
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.BufferCount = 2;
	swap_chain_desc.OutputWindow = GetHwnd(mWnd);
	swap_chain_desc.Windowed = TRUE;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swap_chain_desc.Flags = 0;

	if (FAILED(g_dxgi_factory->CreateSwapChain(g_D3D12Device, &swap_chain_desc, &m_SwapChain))) {
		return false;
	}
	
	if (FAILED(CreateRenderTarget())) {
		return false;
	}
	
	if (FAILED(CreateDepthBuffer())) {
		return false;
	}

	if (FAILED(CreateDefaultCommandQueue())){
		return false;
	}

	return true;
}

// Create Render Target
HRESULT GfxApiDirect3D12::CreateRenderTarget()
{
	// Create descriptor heap for RTV
	D3D12_DESCRIPTOR_HEAP_DESC heapDescRTV = { D3D12_RTV_DESCRIPTOR_HEAP, 1, };
	HRESULT hr = g_D3D12Device->CreateDescriptorHeap(&heapDescRTV, __uuidof(ID3D12DescriptorHeap), (void **)&g_HeapRTV);
	if (FAILED(hr))
		return hr;

	// Get Back Buffer
	hr = m_SwapChain->GetBuffer(g_CurrentBufferId, __uuidof(ID3D12Resource), (void **)&g_BackBuffer);
	if (FAILED(hr))
		return hr;

	// create render target view
	g_D3D12Device->CreateRenderTargetView(g_BackBuffer, 0, g_HeapRTV->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}

// Create Depth Buffer
HRESULT GfxApiDirect3D12::CreateDepthBuffer()
{
	D3D12_CLEAR_VALUE cv;
	cv.DepthStencil.Depth = 1.0f;
	cv.DepthStencil.Stencil = 0;
	cv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	if (FAILED(g_D3D12Device->CreateCommittedResource(
		&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_MISC_NONE,
		&CD3D12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R24G8_TYPELESS, g_ClientWidth, g_ClientHeight, 1, 1, 1, 0, D3D12_RESOURCE_MISC_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_USAGE_INITIAL,
		&cv,
		__uuidof(ID3D12Resource),
		reinterpret_cast<void**>(&g_DepthStencilBuffer))))
	{
		return false;
	}

	// Create descriptor heap for DSV
	D3D12_DESCRIPTOR_HEAP_DESC deapDescDSV = { D3D12_DSV_DESCRIPTOR_HEAP, 1, D3D12_DESCRIPTOR_HEAP_SHADER_VISIBLE };
	HRESULT hr = g_D3D12Device->CreateDescriptorHeap(&deapDescDSV, __uuidof(ID3D12DescriptorHeap), (void **)&g_HeapDSV);
	if (FAILED(hr))
		return hr;

	D3D12_DEPTH_STENCIL_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MipSlice = 0;
	desc.Flags = D3D12_DSV_NONE;
	g_D3D12Device->CreateDepthStencilView(g_DepthStencilBuffer, &desc, g_HeapDSV->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}

// Create Default Command Queue
HRESULT GfxApiDirect3D12::CreateDefaultCommandQueue()
{
	// create command queue
	D3D12_COMMAND_QUEUE_DESC cqd;
	memset(&cqd, 0, sizeof(cqd));
	cqd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	cqd.Flags = D3D12_COMMAND_QUEUE_NONE;
	cqd.Priority = 0;
	cqd.NodeMask = 0;
	HRESULT hr = g_D3D12Device->CreateCommandQueue(&cqd, __uuidof(ID3D12CommandQueue), (void**)&g_CommandQueue);
	if (FAILED(hr))
		return false;

	// create command queue allocator
	hr = g_D3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&g_CommandAllocator);
	if (FAILED(hr))
		return false;

	// Create Command List
	g_D3D12Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, g_CommandAllocator, 0, __uuidof(ID3D12GraphicsCommandList), (void**)&g_CommandList);
	g_CommandList->Close();

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

// Helper routine for resource barriers
void AddResourceBarrier(
	ID3D12GraphicsCommandList * pCl,
	ID3D12Resource * pRes,
	D3D12_RESOURCE_USAGE usageBefore,
	D3D12_RESOURCE_USAGE usageAfter)
{
	D3D12_RESOURCE_BARRIER_DESC barrierDesc =
	{
		D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		{ {
			pRes,
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			usageBefore,
			usageAfter,
		} },
	};
	pCl->ResourceBarrier(1, &barrierDesc);
}