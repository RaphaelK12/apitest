#pragma once

#include "solutions/dynamicstreamingsoln.h"

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
class DynamicStreamingD3D12Bundle : public DynamicStreamingSolution
{
public:
	DynamicStreamingD3D12Bundle();
	virtual ~DynamicStreamingD3D12Bundle();

	virtual bool Init(size_t _maxVertexCount) override;
	virtual void Render(const std::vector<Vec2>& _vertices) override;
	virtual void Shutdown() override;

	virtual std::string GetName() const override { return "D3D12Bundle"; }
	virtual bool SupportsApi(EGfxApi _api) const override { return _api == EGfxApi::Direct3D12; }

private:
	struct Constants
	{
		float width;
		float height;
	};

	comptr<ID3D12PipelineState>			m_PipelineState;
	comptr<ID3D12RootSignature>			m_RootSignature;

	comptr<ID3D12Resource>				m_GeometryBuffer;
	D3D12_VERTEX_BUFFER_VIEW			m_VertexBufferView;

	comptr<ID3D12CommandAllocator>		m_CommandAllocator[NUM_ACCUMULATED_FRAMES];
	comptr<ID3D12GraphicsCommandList>	m_CommandList[NUM_ACCUMULATED_FRAMES];

	comptr<ID3D12CommandAllocator>		m_CommandBundleAllocator[NUM_ACCUMULATED_FRAMES];
	comptr<ID3D12GraphicsCommandList>	m_CommandBundle[NUM_ACCUMULATED_FRAMES];

	size_t								m_BufferSize;
	UINT8*								m_VertexData;

	size_t								kVertexSizeBytes;
	size_t								kParticleCount;
	size_t								kTotalVertices;
	size_t								kOffsetInBytes;
	size_t								kPerticleInBytes;

	bool CreatePSO();
	bool CreateGeometryBuffer(size_t _maxVertexCount);
	bool CreateCommandList();
};
