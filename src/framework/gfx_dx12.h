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

template <typename T>
ID3D12Resource* CreateBufferFromVector(const std::vector<T>& _data, ID3D12Heap* heap, UINT64 offset )
{
	const size_t sizeofVertex = sizeof(T);
	const size_t sizeofVertices = _data.size() * sizeofVertex;

	// the buffer
	ID3D12Resource*	buffer = 0;

	// Create a placed resource that spans the whole heap
	if (FAILED(g_D3D12Device->CreatePlacedResource(
		heap,
		offset,
		&CD3D12_RESOURCE_DESC::Buffer(sizeofVertices),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		__uuidof(ID3D12Resource),
		reinterpret_cast<void**>(&buffer))))
	{
		return 0;
	}

	UINT8* pRaw = 0;
	buffer->Map(0, 0, reinterpret_cast<void**>(&pRaw));
	memcpy(pRaw, _data.data(), sizeofVertices);
	buffer->Unmap(0 , 0);

	return buffer;
}

// heap property wrapper
struct CD3D12_HEAP_PROPERTIES : public D3D12_HEAP_PROPERTIES
{
	CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE type)
	{
		memset(this, 0, sizeof(*this));
		Type = type;
	}
};

// wrapper for heap desc
struct CD3D12_HEAP_DESC : public D3D12_HEAP_DESC
{
	CD3D12_HEAP_DESC(
		UINT64 size,
		D3D12_HEAP_PROPERTIES properties,
		UINT64 alignment,
		D3D12_HEAP_FLAGS miscFlags )
	{
		SizeInBytes = size;
		Properties = properties;
		Alignment = alignment;
		Flags = miscFlags;
	}
};

// wrapper for resource description
struct CD3D12_RESOURCE_DESC
{
	static D3D12_RESOURCE_DESC Buffer(unsigned int size)
	{
		D3D12_RESOURCE_DESC desc;
		memset(&desc, 0, sizeof(desc));
		desc.DepthOrArraySize = 1;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.Width = size;
		desc.Height = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		return desc;
	}

	static D3D12_RESOURCE_DESC Tex2D(DXGI_FORMAT format,
		UINT64                   width,
		UINT                     height,
		UINT16                   arraySize = 1,
		UINT16                   mipLevels = 0,
		UINT                     sampleCount = 1,
		UINT                     sampleQuality = 0,
		D3D12_RESOURCE_FLAGS	 miscFlags = D3D12_RESOURCE_FLAG_NONE,
		D3D12_TEXTURE_LAYOUT     layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		UINT64                   alignment = 0)
	{
		D3D12_RESOURCE_DESC desc;
		memset(&desc, 0, sizeof(desc));
		desc.Width = width;
		desc.Height = height;
		desc.DepthOrArraySize = arraySize;
		desc.MipLevels = mipLevels;
		desc.SampleDesc.Count = sampleCount;
		desc.SampleDesc.Quality = sampleQuality;
		desc.Flags = miscFlags;
		desc.Layout = layout;
		desc.Alignment = alignment;

		return desc;
	}
};

// wrapper for depth stencil state
struct CD3D12_DEPTH_STENCIL_DESC : public D3D12_DEPTH_STENCIL_DESC
{
	CD3D12_DEPTH_STENCIL_DESC()
	{
		memset(this, 0, sizeof(*this));
	}
};