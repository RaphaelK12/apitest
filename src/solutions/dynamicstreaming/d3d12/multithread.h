#pragma once

#include "solutions/dynamicstreamingsoln.h"

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
class DynamicStreamingD3D12MultiThread : public DynamicStreamingSolution
{
public:
	DynamicStreamingD3D12MultiThread();
	virtual ~DynamicStreamingD3D12MultiThread();

	virtual bool Init(size_t _maxVertexCount) override;
	virtual void Render(const std::vector<Vec2>& _vertices) override;
	virtual void Shutdown() override;

	virtual std::string GetName() const override { return "D3D12MultiThread"; }
	virtual bool SupportsApi(EGfxApi _api) const override { return _api == EGfxApi::Direct3D12; }

private:
	/*struct MatrixBuffer
	{
	Matrix m;
	};

	comptr<ID3D12PipelineState>			m_PipelineState;
	comptr<ID3D12RootSignature>			m_RootSignature;

	comptr<ID3D12Resource>				m_GeometryBuffer;
	D3D12_VERTEX_BUFFER_VIEW			m_VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW				m_IndexBufferView;

	size_t								m_IndexCount;
	size_t								m_DescriptorSize;

	comptr<ID3D12CommandAllocator>		m_CommandAllocator[NUM_ACCUMULATED_FRAMES];
	comptr<ID3D12GraphicsCommandList>	m_CommandList[NUM_ACCUMULATED_FRAMES];

	UINT64								m_curFenceValue[NUM_ACCUMULATED_FRAMES];
	int									m_ContextId;

	bool CreatePSO();
	bool CreateGeometryBuffer(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
	const std::vector<UntexturedObjectsProblem::Index>& _indices);
	bool CreateCommandList();*/
};
