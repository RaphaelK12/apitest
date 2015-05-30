#include "pch.h"

#include "gfx_dx12.h"

#pragma comment(lib, "d3d12.lib")

// Globals
comptr<IDXGIFactory>				g_dxgi_factory;
comptr<ID3D12Device>				g_D3D12Device;
comptr<ID3D12CommandQueue>			g_CommandQueue;
comptr<ID3D12CommandAllocator>		g_CommandAllocator[NUM_ACCUMULATED_FRAMES];
comptr<ID3D12DescriptorHeap>		g_HeapRTV;
comptr<ID3D12DescriptorHeap>		g_HeapDSV;
comptr<ID3D12Resource>				g_BackBuffer;
comptr<ID3D12Resource>				g_DepthStencilBuffer;
static comptr<ID3D12CommandAllocator>		g_CopyCommandAllocator;
static comptr<ID3D12GraphicsCommandList>	g_CopyCommand;
int									g_curContext = 0;

int		g_ClientWidth;
int		g_ClientHeight;

// Finish fence
comptr<ID3D12Fence> g_FinishFence;
UINT64 g_finishFenceValue;

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
	if (!GfxBaseApi::Init(_title, _x, _y, _width, _height)) 
	{
		return false;
	}

	g_ClientWidth = _width;
	g_ClientHeight = _height;

	mWnd = SDL_CreateWindow(_title.c_str(), _x, _y, _width, _height, SDL_WINDOW_HIDDEN);

	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&g_dxgi_factory));
	if (FAILED(hr))
		return false;
	/*
	D3D12_CREATE_DEVICE_FLAG flag = D3D12_CREATE_DEVICE_NONE;
#if _DEBUG
	flag |= D3D12_CREATE_DEVICE_DEBUG;
#endif
*/
	// Create D3D12 Device
	hr = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void **)&g_D3D12Device);
	if (FAILED(hr))
		return false;

	// Create Swap Chain
	if (!CreateSwapChain())
		return false;

	// Create a fence
	hr = (g_D3D12Device->CreateFence(g_finishFenceValue, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void**>(&g_FinishFence)));
	if (FAILED(hr))
		return false;

	for (int i = 0; i < NUM_ACCUMULATED_FRAMES; ++i)
		m_curFenceValue[i] = 0;

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

	for (int i = 0; i < NUM_ACCUMULATED_FRAMES; ++i)
	{
		g_CommandAllocator[i].release();
		m_BeginCommandList[i].release();
		m_EndCommandList[i].release();
	}

	g_CommandQueue.release();

	g_CopyCommandAllocator.release();
	g_CopyCommand.release();

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
	// Check out fence
	const UINT64 lastCompletedFence = g_FinishFence->GetCompletedValue();
	if (m_curFenceValue[g_curContext] > lastCompletedFence)
	{
		HANDLE handleEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		g_FinishFence->SetEventOnCompletion(m_curFenceValue[g_curContext], handleEvent);
		WaitForSingleObject(handleEvent, INFINITE);
		CloseHandle(handleEvent);
	}

	HRESULT hr = g_CommandAllocator[g_curContext]->Reset();
	if (FAILED(hr))
		return;

	// reset command list
	m_BeginCommandList[g_curContext]->Reset(g_CommandAllocator[g_curContext], 0);

	// Add resource barrier
	AddResourceBarrier(m_BeginCommandList[g_curContext], g_BackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	// Bind render target for drawing
	m_BeginCommandList[g_curContext]->ClearRenderTargetView(g_HeapRTV->GetCPUDescriptorHandleForHeapStart(), &_clearColor.x, 0, 0);

	// Clear depth buffer
	m_BeginCommandList[g_curContext]->ClearDepthStencilView(g_HeapDSV->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, 0);

	// Close the event list
	m_BeginCommandList[g_curContext]->Close();

	// Execute command
	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_BeginCommandList[g_curContext]);
}

// --------------------------------------------------------------------------------------------------------------------
void GfxApiDirect3D12::SwapBuffers()
{
	// Reset command list
	m_EndCommandList[g_curContext]->Reset(g_CommandAllocator[g_curContext], 0);

	// Add resource barrier
	AddResourceBarrier(m_EndCommandList[g_curContext], g_BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	// Close the command list
	m_EndCommandList[g_curContext]->Close();

	// execute command list
	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_EndCommandList[g_curContext]);

	// Present
	m_SwapChain->Present(0, 0);

	// setup fence
	m_curFenceValue[g_curContext] = ++g_finishFenceValue;
	g_CommandQueue->Signal(g_FinishFence, g_finishFenceValue);

	g_curContext = (g_curContext + 1) % NUM_ACCUMULATED_FRAMES;
}

// --------------------------------------------------------------------------------------------------------------------
bool GfxApiDirect3D12::CreateSwapChain()
{
	if (FAILED(CreateDefaultCommandQueue())){
		return false;
	}

	// Create Swap Chain
	DXGI_SWAP_CHAIN_DESC swap_chain_desc;
	memset(&swap_chain_desc, 0, sizeof(swap_chain_desc));
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

	if (FAILED(g_dxgi_factory->CreateSwapChain(g_CommandQueue, &swap_chain_desc, &m_SwapChain))) {
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
HRESULT GfxApiDirect3D12::CreateRenderTarget()
{
	// Create descriptor heap for RTV
	D3D12_DESCRIPTOR_HEAP_DESC heapDescRTV = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
	HRESULT hr = g_D3D12Device->CreateDescriptorHeap(&heapDescRTV, __uuidof(ID3D12DescriptorHeap), (void **)&g_HeapRTV);
	if (FAILED(hr))
		return hr;

	// Get Back Buffer
	hr = m_SwapChain->GetBuffer(0, __uuidof(ID3D12Resource), (void **)&g_BackBuffer);
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
		D3D12_HEAP_FLAG_NONE,
		&CD3D12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R24G8_TYPELESS, g_ClientWidth, g_ClientHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_COMMON,
		&cv,
		__uuidof(ID3D12Resource),
		reinterpret_cast<void**>(&g_DepthStencilBuffer))))
	{
		return false;
	}

	// Create descriptor heap for DSV
	D3D12_DESCRIPTOR_HEAP_DESC deapDescDSV = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE };
	HRESULT hr = g_D3D12Device->CreateDescriptorHeap(&deapDescDSV, __uuidof(ID3D12DescriptorHeap), (void **)&g_HeapDSV);
	if (FAILED(hr))
		return hr;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc;
	memset(&dsv_desc, 0, sizeof(dsv_desc));
	dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv_desc.Texture2D.MipSlice = 0;
	dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
	g_D3D12Device->CreateDepthStencilView(g_DepthStencilBuffer, &dsv_desc, g_HeapDSV->GetCPUDescriptorHandleForHeapStart());

	// use copy command list temporarily
	g_CopyCommandAllocator->Reset();
	g_CopyCommand->Reset(g_CopyCommandAllocator, 0);
	AddResourceBarrier(g_CopyCommand, g_DepthStencilBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	g_CopyCommand->Close();
	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_CopyCommand);

	return S_OK;
}

// Create Default Command Queue
HRESULT GfxApiDirect3D12::CreateDefaultCommandQueue()
{
	// create command queue
	D3D12_COMMAND_QUEUE_DESC cqd;
	memset(&cqd, 0, sizeof(cqd));
	cqd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	cqd.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cqd.Priority = 0;
	cqd.NodeMask = 0;
	HRESULT hr = g_D3D12Device->CreateCommandQueue(&cqd, __uuidof(ID3D12CommandQueue), (void**)&g_CommandQueue);
	if (FAILED(hr))
		return false;

	for (int i = 0; i < NUM_ACCUMULATED_FRAMES; ++i)
	{
		// create command queue allocator
		hr = g_D3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&g_CommandAllocator[i]);
		if (FAILED(hr))
			return false;

		g_CommandAllocator[i]->SetName(L"GFX_CommandAllocator");

		// Create Command List
		g_D3D12Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, g_CommandAllocator[i], 0, __uuidof(ID3D12GraphicsCommandList), (void**)&m_BeginCommandList[i]);
		m_BeginCommandList[i]->Close();

		m_BeginCommandList[i]->SetName(L"BeginCommandList");

		// Create Command List
		g_D3D12Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, g_CommandAllocator[i], 0, __uuidof(ID3D12GraphicsCommandList), (void**)&m_EndCommandList[i]);
		m_EndCommandList[i]->Close();

		m_BeginCommandList[i]->SetName(L"EndCommandList");
	}

	// create command queue allocator
	hr = g_D3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&g_CopyCommandAllocator);
	if (FAILED(hr))
			return false;

	// Create Command List
	g_D3D12Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, g_CopyCommandAllocator, 0, __uuidof(ID3D12GraphicsCommandList), (void**)&g_CopyCommand);
	g_CopyCommand->Close();

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
	D3D12_RESOURCE_STATES usageBefore,
	D3D12_RESOURCE_STATES usageAfter)
{

	D3D12_RESOURCE_BARRIER barrier = {
		D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		D3D12_RESOURCE_BARRIER_FLAG_NONE,
		{{
			pRes,
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			usageBefore,
			usageAfter,
		}},
	};
	pCl->ResourceBarrier(1, &barrier);
}

ID3D12Resource* NewTextureFromDetails(const TextureDetails& _texDetails)
{
	ID3D12Resource*	texture = 0;

	D3D12_RESOURCE_DESC texdesc;
	memset(&texdesc, 0, sizeof(texdesc));
	texdesc.DepthOrArraySize = 1;
	texdesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texdesc.Format = (DXGI_FORMAT)_texDetails.d3dFormat;
	texdesc.Width = _texDetails.dwWidth;
	texdesc.Height = _texDetails.dwHeight;
	texdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texdesc.MipLevels = _texDetails.szMipMapCount;
	texdesc.SampleDesc.Count = 1;
	texdesc.SampleDesc.Quality = 0;
	
	// create the default texture
	HRESULT hr = g_D3D12Device->CreateCommittedResource(
		&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texdesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		__uuidof(ID3D12Resource),
		(void**)&texture);
	if (FAILED(hr))
		return 0;

	UINT64 uploadBufferSize;
	{
		const UINT num2DSubresources = texdesc.DepthOrArraySize * texdesc.MipLevels;
		D3D12_RESOURCE_DESC Desc = texture->GetDesc();
		ID3D12Device* pDevice;
		texture->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));
		pDevice->GetCopyableFootprints(&Desc, 0, num2DSubresources, 0, nullptr, nullptr, nullptr, &uploadBufferSize);
		pDevice->Release();
	}

	// create an immediate upload heap
	const UINT num2DSubresources = texdesc.DepthOrArraySize * texdesc.MipLevels;
	comptr<ID3D12Resource>	uploadTex;
	hr = g_D3D12Device->CreateCommittedResource(
		&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3D12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		__uuidof(ID3D12Resource),
		(void**)&uploadTex);
	if (FAILED(hr))
	{
		texture->Release();
		return 0;
	}

	// copy data to the intermediate upload heap and then schedule a copy from the upload heap to the default texture
	D3D12_SUBRESOURCE_DATA texResource = {};
	texResource.pData = static_cast<const void*>(_texDetails.pPixels);
	texResource.RowPitch = _texDetails.pPitches[0];
	texResource.SlicePitch = 0;

	size_t accumulatedMipOffset = 0;
	std::vector<D3D12_SUBRESOURCE_DATA> initialDatas;
	for (UINT mip = 0; mip < texdesc.MipLevels; ++mip) {
		D3D12_SUBRESOURCE_DATA mipData;
		mipData.pData = &((unsigned char*)_texDetails.pPixels)[accumulatedMipOffset];
		mipData.RowPitch = _texDetails.pPitches[mip];
		mipData.SlicePitch = 0;

		initialDatas.push_back(mipData);
		// Bump the mip offset for the next level.
		accumulatedMipOffset += _texDetails.pSizes[mip];
	}

	g_CopyCommandAllocator->Reset();
	g_CopyCommand->Reset(g_CopyCommandAllocator, 0);

	AddResourceBarrier(g_CopyCommand, texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
	//UpdateSubresources(g_CopyCommand, texture, uploadTex, 0, 0, num2DSubresources, initialDatas.data());
	AddResourceBarrier(g_CopyCommand, texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	g_CopyCommand->Close();

	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_CopyCommand);

	// Make sure everything is flushed out before releasing anything.
	HANDLE handleEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
	g_CommandQueue->Signal(g_FinishFence, ++g_finishFenceValue);
	g_FinishFence->SetEventOnCompletion(g_finishFenceValue, handleEvent);
	WaitForSingleObject(handleEvent, INFINITE);
	CloseHandle(handleEvent);

	// release the upload texture
	uploadTex.release();
	
	return texture;
}