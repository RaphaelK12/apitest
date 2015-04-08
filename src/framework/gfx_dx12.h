#pragma once

#include "gfx.h"

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
	IDXGISwapChain* m_SwapChain;

	// Root Signature
	ID3D12RootSignature*	m_RootSignature;
	// Command List
	ID3D12GraphicsCommandList*	m_CommandList;

	// Create Swap Chain
	bool CreateSwapChain();
	// Create Render Target
	HRESULT CreateRenderTarget();
	// Create Depth Buffer
	HRESULT CreateDepthBuffer();
	// Create Default PSO
	HRESULT CreateDefaultPSO();
	// Create Default Command Queue
	HRESULT CreateDefaultCommandQueue();
};

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// DX12 Utilities
extern ID3D12Device*			g_D3D12Device;
extern ID3D12CommandQueue*		g_CommandQueue;
extern ID3D12CommandAllocator*	g_CommandAllocator;
extern ID3D12PipelineState*		g_DefaultPSO;

// Helper routine for resource barriers
void AddResourceBarrier(
	ID3D12GraphicsCommandList * pCl,
	ID3D12Resource * pRes,
	D3D12_RESOURCE_USAGE usageBefore,
	D3D12_RESOURCE_USAGE usageAfter);

template <typename T>
ID3D12Resource* CreateBufferFromVector(const std::vector<T>& _data, ID3D12Heap* heap , UINT64 offset )
{
	const size_t sizeofVertex = sizeof(T);
	const size_t sizeofVertices = _data.size() * sizeofVertex;
	/*
	The whole buffer is created in the wrong way....
	ID3D12Resource*	buffer = 0;
	if (FAILED(g_D3D12Device->CreateCommittedResource(
		&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_MISC_NONE,
		&CD3D12_RESOURCE_DESC::Buffer(sizeofVertices),
		D3D12_RESOURCE_USAGE_GENERIC_READ,
		nullptr,
		__uuidof(ID3D12Resource),
		reinterpret_cast<void**>(&buffer))))
	{
		return 0;
	}

	void* pVertices = nullptr;
	buffer->Map(0, nullptr, reinterpret_cast<void**>(&pVertices));
	memcpy(pVertices, _data.data(), sizeofVertices);
	buffer->Unmap(0, nullptr);
	*/

	//HRESULT hr = g_D3D12Device->CreateHeap(&CD3D12_HEAP_DESC(s_vertexBufferSize, D3D12_HEAP_TYPE_UPLOAD, 0, D3D12_HEAP_MISC_DENY_TEXTURES),	__uuidof(ID3D12Heap),reinterpret_cast<void**>(&m_pVertexBufferHeap))

	// the buffer
	ID3D12Resource*	buffer = 0;

	UINT32 buffer_size = _data.size() * sizeof(UntexturedObjectsProblem::Vertex);
	// Create a placed resource that spans the whole heap
	if (FAILED(g_D3D12Device->CreatePlacedResource(
		heap,
		offset,
		&CD3D12_RESOURCE_DESC::Buffer(buffer_size),
		D3D12_RESOURCE_USAGE_GENERIC_READ,
		nullptr,
		__uuidof(ID3D12Resource),
		reinterpret_cast<void**>(&buffer))))
	{
		return 0;
	}

	UINT8* pRaw = 0;
	buffer->Map(0, 0, reinterpret_cast<void**>(&pRaw));
	memcpy(pRaw, _data.data(), buffer_size);
	buffer->Unmap(0 , 0);

	return buffer;
}