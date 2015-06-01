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

//------------------------------------------------------------------------------------------------------------------------------------
// D3D12 v4 legacy code

// heap property wrapper
struct CD3D12_HEAP_PROPERTIES : public D3D12_HEAP_PROPERTIES
{
	CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE type)
	{
		memset(this, 0, sizeof(*this));
		Type = type;

		CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		CreationNodeMask = 1;
		VisibleNodeMask = 1;
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

struct CD3D12_RESOURCE_DESC : public D3D12_RESOURCE_DESC
{
	CD3D12_RESOURCE_DESC()
	{}
	explicit CD3D12_RESOURCE_DESC(const D3D12_RESOURCE_DESC& o) :
		D3D12_RESOURCE_DESC(o)
	{}
	CD3D12_RESOURCE_DESC(
		D3D12_RESOURCE_DIMENSION dimension,
		UINT64 alignment,
		UINT64 width,
		UINT height,
		UINT16 depthOrArraySize,
		UINT16 mipLevels,
		DXGI_FORMAT format,
		UINT sampleCount,
		UINT sampleQuality,
		D3D12_TEXTURE_LAYOUT layout,
		D3D12_RESOURCE_FLAGS miscFlags)
	{
		Dimension = dimension;
		Alignment = alignment;
		Width = width;
		Height = height;
		DepthOrArraySize = depthOrArraySize;
		MipLevels = mipLevels;
		Format = format;
		SampleDesc.Count = sampleCount;
		SampleDesc.Quality = sampleQuality;
		Layout = layout;
		Flags = miscFlags;
	}
	static CD3D12_RESOURCE_DESC Buffer(
		const D3D12_RESOURCE_ALLOCATION_INFO& resAllocInfo,
		D3D12_RESOURCE_FLAGS miscFlags = D3D12_RESOURCE_FLAG_NONE)
	{
		return CD3D12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_BUFFER, resAllocInfo.Alignment, resAllocInfo.SizeInBytes,
			1, 1, 1, DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, miscFlags);
	}
	static CD3D12_RESOURCE_DESC Buffer(
		UINT64 width,
		D3D12_RESOURCE_FLAGS miscFlags = D3D12_RESOURCE_FLAG_NONE,
		UINT64 alignment = 0)
	{
		return CD3D12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_BUFFER, alignment, width, 1, 1, 1,
			DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, miscFlags);
	}
	static CD3D12_RESOURCE_DESC Tex1D(
		DXGI_FORMAT format,
		UINT64 width,
		UINT16 arraySize = 1,
		UINT16 mipLevels = 0,
		D3D12_RESOURCE_FLAGS miscFlags = D3D12_RESOURCE_FLAG_NONE,
		D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		UINT64 alignment = 0)
	{
		return CD3D12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE1D, alignment, width, 1, arraySize,
			mipLevels, format, 1, 0, layout, miscFlags);
	}
	static CD3D12_RESOURCE_DESC Tex2D(
		DXGI_FORMAT format,
		UINT64 width,
		UINT height,
		UINT16 arraySize = 1,
		UINT16 mipLevels = 0,
		UINT sampleCount = 1,
		UINT sampleQuality = 0,
		D3D12_RESOURCE_FLAGS miscFlags = D3D12_RESOURCE_FLAG_NONE,
		D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		UINT64 alignment = 0)
	{
		return CD3D12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE2D, alignment, width, height, arraySize,
			mipLevels, format, sampleCount, sampleQuality, layout, miscFlags);
	}
	static CD3D12_RESOURCE_DESC Tex3D(
		DXGI_FORMAT format,
		UINT64 width,
		UINT height,
		UINT16 depth,
		UINT16 mipLevels = 0,
		D3D12_RESOURCE_FLAGS miscFlags = D3D12_RESOURCE_FLAG_NONE,
		D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		UINT64 alignment = 0)
	{
		return CD3D12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE3D, alignment, width, height, depth,
			mipLevels, format, 1, 0, layout, miscFlags);
	}
	UINT16 Depth() const
	{
		return (Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? DepthOrArraySize : 1);
	}
	operator const D3D12_RESOURCE_DESC&() const { return *this; }
};

// wrapper for depth stencil state
struct CD3D12_DEPTH_STENCIL_DESC : public D3D12_DEPTH_STENCIL_DESC
{
	CD3D12_DEPTH_STENCIL_DESC()
	{
		memset(this, 0, sizeof(*this));
	}
};

struct CD3D12_TEXTURE_COPY_LOCATION : public D3D12_TEXTURE_COPY_LOCATION
{
	CD3D12_TEXTURE_COPY_LOCATION() { }
	CD3D12_TEXTURE_COPY_LOCATION(ID3D12Resource* pRes) { pResource = pRes; }
	CD3D12_TEXTURE_COPY_LOCATION(ID3D12Resource* pRes, D3D12_PLACED_SUBRESOURCE_FOOTPRINT const& Placement)
	{
		pResource = pRes;
		Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		PlacedFootprint = Placement;
	}
	CD3D12_TEXTURE_COPY_LOCATION(ID3D12Resource* pRes, UINT Sub)
	{
		pResource = pRes;
		Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		SubresourceIndex = Sub;
	}
};

// Row-by-row memcpy
inline void MemcpySubresource(
	const D3D12_MEMCPY_DEST* pDest,
	const D3D12_SUBRESOURCE_DATA* pSrc,
	SIZE_T RowSizeInBytes,
	UINT NumRows,
	UINT NumSlices)
{
	for (UINT z = 0; z < NumSlices; ++z)
	{
		BYTE* pDestSlice = reinterpret_cast<BYTE*>(pDest->pData) + pDest->SlicePitch * z;
		const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(pSrc->pData) + pSrc->SlicePitch * z;
		for (UINT y = 0; y < NumRows; ++y)
		{
			memcpy(pDestSlice + pDest->RowPitch * y,
				pSrcSlice + pSrc->RowPitch * y,
				RowSizeInBytes);
		}
	}
}

// All arrays must be populated (e.g. by calling GetCopyableLayout)
inline UINT64 UpdateSubresources(
	_In_ ID3D12GraphicsCommandList* pCmdList,
	_In_ ID3D12Resource* pDestinationResource,
	_In_ ID3D12Resource* pIntermediate,
	_In_range_(0, D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
	_In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource) UINT NumSubresources,
	UINT64 RequiredSize,
	_In_reads_(NumSubresources) const D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts,
	_In_reads_(NumSubresources) const UINT* pNumRows,
	_In_reads_(NumSubresources) const UINT64* pRowSizesInBytes,
	_In_reads_(NumSubresources) const D3D12_SUBRESOURCE_DATA* pSrcData)
{
	// Minor validation
	D3D12_RESOURCE_DESC IntermediateDesc = pIntermediate->GetDesc();
	D3D12_RESOURCE_DESC DestinationDesc = pDestinationResource->GetDesc();
	if (IntermediateDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
		IntermediateDesc.Width < RequiredSize + pLayouts[0].Offset ||
		RequiredSize >(SIZE_T)-1 ||
		(DestinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER &&
		(FirstSubresource != 0 || NumSubresources != 1)))
	{
		return 0;
	}

	BYTE* pData;
	HRESULT hr = pIntermediate->Map(0, NULL, reinterpret_cast<void**>(&pData));
	if (FAILED(hr))
	{
		return 0;
	}

	for (UINT i = 0; i < NumSubresources; ++i)
	{
		if (pRowSizesInBytes[i] >(SIZE_T)-1) return 0;
		D3D12_MEMCPY_DEST DestData = { pData + pLayouts[i].Offset, pLayouts[i].Footprint.RowPitch, pLayouts[i].Footprint.RowPitch * pNumRows[i] };
		MemcpySubresource(&DestData, &pSrcData[i], (SIZE_T)pRowSizesInBytes[i], pNumRows[i], pLayouts[i].Footprint.Depth);
	}
	pIntermediate->Unmap(0, NULL);

	{
		for (UINT i = 0; i < NumSubresources; ++i)
		{
			CD3D12_TEXTURE_COPY_LOCATION Dst(pDestinationResource, i + FirstSubresource);
			CD3D12_TEXTURE_COPY_LOCATION Src(pIntermediate, pLayouts[i]);
			pCmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
		}
	}
	return RequiredSize;
}


// Heap-allocating UpdateSubresources implementation
inline UINT64 UpdateSubresources(
	_In_ ID3D12GraphicsCommandList* pCmdList,
	_In_ ID3D12Resource* pDestinationResource,
	_In_ ID3D12Resource* pIntermediate,
	UINT64 IntermediateOffset,
	_In_range_(0, D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
	_In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource) UINT NumSubresources,
	_In_reads_(NumSubresources) D3D12_SUBRESOURCE_DATA* pSrcData)
{
	UINT64 RequiredSize = 0;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts = new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[NumSubresources];
	UINT* pNumRows = new UINT[NumSubresources];
	UINT64* pRowSizesInBytes = new UINT64[NumSubresources];

	D3D12_RESOURCE_DESC Desc = pDestinationResource->GetDesc();
	ID3D12Device* pDevice;
	pDestinationResource->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));
	pDevice->GetCopyableFootprints(&Desc, FirstSubresource, NumSubresources, IntermediateOffset, pLayouts, pNumRows, pRowSizesInBytes, &RequiredSize);
	pDevice->Release();

	UINT64 Result = UpdateSubresources(pCmdList, pDestinationResource, pIntermediate, FirstSubresource, NumSubresources, RequiredSize, pLayouts, pNumRows, pRowSizesInBytes, pSrcData);
	delete[] pLayouts;
	delete[] pNumRows;
	delete[] pRowSizesInBytes;
	return 0;
}
