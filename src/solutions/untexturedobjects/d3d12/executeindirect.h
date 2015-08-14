#pragma once

#include "solutions/untexturedobjectssoln.h"

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
class UntexturedObjectsD3D12ExecuteIndirect : public UntexturedObjectsSolution
{
public:
	UntexturedObjectsD3D12ExecuteIndirect();
	virtual ~UntexturedObjectsD3D12ExecuteIndirect() { }

	virtual bool Init(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
		const std::vector<UntexturedObjectsProblem::Index>& _indices,
		size_t _objectCount) override;

	virtual void Render(const std::vector<Matrix>& _transforms);
	virtual void Shutdown();

	virtual std::string GetName() const { return "ExecuteIndirect"; }

	virtual bool SupportsApi(EGfxApi _api) const override { return _api == EGfxApi::Direct3D12; }

private:
	struct MatrixBuffer
	{
		Matrix m;
	};

	comptr<ID3D12PipelineState>			m_PipelineState;
	comptr<ID3D12RootSignature>			m_RootSignature;

	comptr<ID3D12Heap>					m_GeometryBufferHeap;
	comptr<ID3D12Resource>				m_GeometryBuffer;
	comptr<ID3D12Resource>				m_GeometryBufferDefault;
	D3D12_VERTEX_BUFFER_VIEW			m_VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW				m_IndexBufferView;

	comptr<ID3D12Resource>				m_ConstantBuffer[NUM_ACCUMULATED_FRAMES];
	MatrixBuffer*						m_ConstantBufferData[NUM_ACCUMULATED_FRAMES];

	size_t								m_IndexCount;

	comptr<ID3D12Resource>				m_CommandBuffer[NUM_ACCUMULATED_FRAMES];
	comptr<ID3D12Resource>				m_CommandBufferDefault[NUM_ACCUMULATED_FRAMES];
	comptr<ID3D12CommandSignature>		m_CommandSig[NUM_ACCUMULATED_FRAMES];

	comptr<ID3D12CommandAllocator>		m_CommandAllocator[NUM_ACCUMULATED_FRAMES];
	comptr<ID3D12GraphicsCommandList>	m_CommandList[NUM_ACCUMULATED_FRAMES];

	comptr<ID3D12CommandAllocator>		m_CopyCommandAllocator;
	comptr<ID3D12GraphicsCommandList>	m_CopyCommand;

	bool CreatePSO();
	bool CreateGeometryBuffer(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
		const std::vector<UntexturedObjectsProblem::Index>& _indices);
	bool CreateConstantBuffer(size_t count);
	bool CreateCommandSignature(size_t count);
	bool CreateCommandList();
};
