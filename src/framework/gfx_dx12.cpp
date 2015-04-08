#include "pch.h"

#include "gfx_dx12.h"

#pragma comment(lib, "d3d12.lib")

// Globals
IDXGIFactory*			g_dxgi_factory = 0;
ID3D12Device*			g_D3D12Device = 0;
ID3D12CommandQueue*		g_CommandQueue = 0;
ID3D12CommandAllocator*	g_CommandAllocator = 0;
ID3D12PipelineState*	g_DefaultPSO =0;
ID3D12DescriptorHeap*	g_HeapRTV = 0;
ID3D12DescriptorHeap*	g_HeapDSV = 0;
ID3D12Resource*			g_BackBuffer = 0;
ID3D12Resource*			g_DepthStencilBuffer = 0;

int		g_ClientWidth;
int		g_ClientHeight;

GfxBaseApi *CreateGfxDirect3D12() { return new GfxApiDirect3D12; }

static HWND GetHwnd(SDL_Window* _wnd);

// --------------------------------------------------------------------------------------------------------------------
const BYTE g_DefaultVertexShader[] =
{
	68, 88, 66, 67, 106, 7,
	45, 78, 75, 221, 41, 234,
	53, 241, 205, 51, 123, 54,
	178, 189, 1, 0, 0, 0,
	116, 2, 0, 0, 5, 0,
	0, 0, 52, 0, 0, 0,
	168, 0, 0, 0, 220, 0,
	0, 0, 16, 1, 0, 0,
	216, 1, 0, 0, 82, 68,
	69, 70, 108, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	60, 0, 0, 0, 0, 5,
	254, 255, 0, 1, 0, 0,
	60, 0, 0, 0, 82, 68,
	49, 49, 60, 0, 0, 0,
	24, 0, 0, 0, 32, 0,
	0, 0, 40, 0, 0, 0,
	36, 0, 0, 0, 12, 0,
	0, 0, 0, 0, 0, 0,
	77, 105, 99, 114, 111, 115,
	111, 102, 116, 32, 40, 82,
	41, 32, 72, 76, 83, 76,
	32, 83, 104, 97, 100, 101,
	114, 32, 67, 111, 109, 112,
	105, 108, 101, 114, 32, 49,
	48, 46, 48, 46, 49, 48,
	48, 49, 49, 46, 48, 0,
	73, 83, 71, 78, 44, 0,
	0, 0, 1, 0, 0, 0,
	8, 0, 0, 0, 32, 0,
	0, 0, 0, 0, 0, 0,
	6, 0, 0, 0, 1, 0,
	0, 0, 0, 0, 0, 0,
	1, 1, 0, 0, 83, 86,
	95, 86, 101, 114, 116, 101,
	120, 73, 68, 0, 79, 83,
	71, 78, 44, 0, 0, 0,
	1, 0, 0, 0, 8, 0,
	0, 0, 32, 0, 0, 0,
	0, 0, 0, 0, 1, 0,
	0, 0, 3, 0, 0, 0,
	0, 0, 0, 0, 15, 0,
	0, 0, 83, 86, 95, 80,
	111, 115, 105, 116, 105, 111,
	110, 0, 83, 72, 69, 88,
	192, 0, 0, 0, 80, 0,
	1, 0, 48, 0, 0, 0,
	106, 8, 0, 1, 96, 0,
	0, 4, 18, 16, 16, 0,
	0, 0, 0, 0, 6, 0,
	0, 0, 103, 0, 0, 4,
	242, 32, 16, 0, 0, 0,
	0, 0, 1, 0, 0, 0,
	104, 0, 0, 2, 1, 0,
	0, 0, 32, 0, 0, 7,
	18, 0, 16, 0, 0, 0,
	0, 0, 10, 16, 16, 0,
	0, 0, 0, 0, 1, 64,
	0, 0, 1, 0, 0, 0,
	55, 0, 0, 15, 242, 0,
	16, 0, 0, 0, 0, 0,
	6, 0, 16, 0, 0, 0,
	0, 0, 2, 64, 0, 0,
	0, 0, 0, 63, 205, 204,
	204, 189, 0, 0, 0, 0,
	0, 0, 128, 63, 2, 64,
	0, 0, 205, 204, 204, 61,
	51, 51, 51, 63, 0, 0,
	0, 0, 0, 0, 128, 63,
	55, 0, 0, 12, 242, 32,
	16, 0, 0, 0, 0, 0,
	6, 16, 16, 0, 0, 0,
	0, 0, 70, 14, 16, 0,
	0, 0, 0, 0, 2, 64,
	0, 0, 205, 204, 204, 190,
	154, 153, 25, 191, 0, 0,
	0, 0, 0, 0, 128, 63,
	62, 0, 0, 1, 83, 84,
	65, 84, 148, 0, 0, 0,
	4, 0, 0, 0, 1, 0,
	0, 0, 0, 0, 0, 0,
	2, 0, 0, 0, 0, 0,
	0, 0, 1, 0, 0, 0,
	0, 0, 0, 0, 1, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 2, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0
};

const BYTE g_DefaultPixelShader[] =
{
	68, 88, 66, 67, 236, 83,
	246, 214, 27, 156, 29, 136,
	158, 51, 103, 58, 101, 182,
	85, 214, 1, 0, 0, 0,
	204, 1, 0, 0, 5, 0,
	0, 0, 52, 0, 0, 0,
	168, 0, 0, 0, 184, 0,
	0, 0, 236, 0, 0, 0,
	48, 1, 0, 0, 82, 68,
	69, 70, 108, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	60, 0, 0, 0, 0, 5,
	255, 255, 0, 1, 0, 0,
	60, 0, 0, 0, 82, 68,
	49, 49, 60, 0, 0, 0,
	24, 0, 0, 0, 32, 0,
	0, 0, 40, 0, 0, 0,
	36, 0, 0, 0, 12, 0,
	0, 0, 0, 0, 0, 0,
	77, 105, 99, 114, 111, 115,
	111, 102, 116, 32, 40, 82,
	41, 32, 72, 76, 83, 76,
	32, 83, 104, 97, 100, 101,
	114, 32, 67, 111, 109, 112,
	105, 108, 101, 114, 32, 49,
	48, 46, 48, 46, 49, 48,
	48, 49, 49, 46, 48, 0,
	73, 83, 71, 78, 8, 0,
	0, 0, 0, 0, 0, 0,
	8, 0, 0, 0, 79, 83,
	71, 78, 44, 0, 0, 0,
	1, 0, 0, 0, 8, 0,
	0, 0, 32, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 3, 0, 0, 0,
	0, 0, 0, 0, 7, 8,
	0, 0, 83, 86, 95, 84,
	97, 114, 103, 101, 116, 0,
	171, 171, 83, 72, 69, 88,
	60, 0, 0, 0, 80, 0,
	0, 0, 15, 0, 0, 0,
	106, 8, 0, 1, 101, 0,
	0, 3, 114, 32, 16, 0,
	0, 0, 0, 0, 54, 0,
	0, 8, 114, 32, 16, 0,
	0, 0, 0, 0, 2, 64,
	0, 0, 205, 204, 76, 63,
	0, 0, 0, 63, 0, 0,
	0, 63, 0, 0, 0, 0,
	62, 0, 0, 1, 83, 84,
	65, 84, 148, 0, 0, 0,
	2, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0
};

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
GfxApiDirect3D12::GfxApiDirect3D12():
m_CommandList()
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

	return true;
}

// --------------------------------------------------------------------------------------------------------------------
void GfxApiDirect3D12::Shutdown()
{
	SafeRelease(m_CommandList);
	SafeRelease(g_CommandAllocator);
	SafeRelease(g_CommandQueue);

	SafeRelease(g_DefaultPSO);
	SafeRelease(m_RootSignature);
	SafeRelease(g_BackBuffer);
	SafeRelease(g_HeapRTV);
	SafeRelease(g_HeapDSV);
	SafeRelease(m_SwapChain);

	SafeRelease(g_D3D12Device);
	SafeRelease(g_dxgi_factory);

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

	// Create Command List first time invoked
	{
		// reset command list
		m_CommandList->Reset(g_CommandAllocator, 0);

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
		m_CommandList->ResourceBarrier(1, &barrierDesc);

		// Bind render target for drawing
		m_CommandList->ClearRenderTargetView(g_HeapRTV->GetCPUDescriptorHandleForHeapStart(), &_clearColor.x, 0, 0);

		// Clear depth buffer
		m_CommandList->ClearDepthStencilView(g_HeapDSV->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_DEPTH, 1.0f, 0, 0, 0);

		// Close the command list
		m_CommandList->Close();
	}

	// Execute Command List
	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_CommandList);
}

// --------------------------------------------------------------------------------------------------------------------
void GfxApiDirect3D12::SwapBuffers()
{
	// Create Command List first time invoked
	{
		// reset command list
		m_CommandList->Reset(g_CommandAllocator, 0);

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
		m_CommandList->ResourceBarrier(1, &barrierDesc);

		// Close the command list
		m_CommandList->Close();
	}

	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_CommandList);

	// Present
	m_SwapChain->Present(0, 0);
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
	swap_chain_desc.BufferCount = 1;
	swap_chain_desc.OutputWindow = GetHwnd(mWnd);
	swap_chain_desc.Windowed = TRUE;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
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

	if (FAILED(CreateDefaultPSO())) {
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
	hr = m_SwapChain->GetBuffer(0, __uuidof(ID3D12Resource), (void **)&g_BackBuffer);
	if (FAILED(hr))
		return hr;

	// create render target view
	g_D3D12Device->CreateRenderTargetView(g_BackBuffer, 0, g_HeapRTV->GetCPUDescriptorHandleForHeapStart());

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
	hr = g_D3D12Device->CreateDescriptorHeap(&deapDescDSV, __uuidof(ID3D12DescriptorHeap), (void **)&g_HeapDSV);
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

// Create Depth Buffer
HRESULT GfxApiDirect3D12::CreateDepthBuffer()
{
	return S_OK;
}

// Create Default PSO
HRESULT GfxApiDirect3D12::CreateDefaultPSO()
{
	D3D12_ROOT_SIGNATURE rootSig = {};
	ID3DBlob* pBlobRootSig, *pBlobErrors;
	HRESULT hr = (D3D12SerializeRootSignature(&rootSig, D3D_ROOT_SIGNATURE_V1, &pBlobRootSig, &pBlobErrors));
	if (FAILED(hr))
		return false;

	hr = (g_D3D12Device->CreateRootSignature(0, pBlobRootSig->GetBufferPointer(), pBlobRootSig->GetBufferSize(), __uuidof(ID3D12RootSignature), (void **)&m_RootSignature));
	if (FAILED(hr))
		return false;

	// Create Pipeline State Object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psod;
	memset(&psod, 0, sizeof(psod));
	psod.pRootSignature = m_RootSignature;
	psod.VS.BytecodeLength = sizeof(g_DefaultVertexShader);
	psod.VS.pShaderBytecode = g_DefaultVertexShader;
	psod.PS.BytecodeLength = sizeof(g_DefaultPixelShader);
	psod.PS.pShaderBytecode = g_DefaultPixelShader;
	psod.RasterizerState.FillMode = D3D12_FILL_SOLID;
	psod.RasterizerState.CullMode = D3D12_CULL_NONE;
	psod.InputLayout.NumElements = 0;
	psod.RasterizerState.FrontCounterClockwise = true;
	psod.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psod.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psod.SampleDesc.Count = 1;
	psod.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psod.NumRenderTargets = 1;
	psod.SampleMask = 0xffffffff;

	return (g_D3D12Device->CreateGraphicsPipelineState(&psod, __uuidof(ID3D12PipelineState), (void**)&g_DefaultPSO));
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

	// Command list
	g_D3D12Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, g_CommandAllocator, g_DefaultPSO, __uuidof(ID3D12CommandList), (void**)&m_CommandList);
	m_CommandList->Close();

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