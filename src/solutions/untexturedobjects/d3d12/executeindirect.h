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
	comptr<ID3D12Resource>				m_VertexBuffer;
	comptr<ID3D12Resource>				m_IndexBuffer;
	D3D12_VERTEX_BUFFER_VIEW			m_VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW				m_IndexBufferView;

	comptr<ID3D12Resource>				m_ConstantBuffer;
	MatrixBuffer*						m_ConstantBufferData;

	size_t								m_IndexCount;
	size_t								m_DescriptorSize;

	comptr<ID3D12Resource>				m_CommandBuffer;
	comptr<ID3D12CommandSignature>		m_CommandSig;

	bool CreatePSO();
	bool CreateGeometryBuffer(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
		const std::vector<UntexturedObjectsProblem::Index>& _indices);
	bool CreateConstantBuffer(size_t count);
	bool CreateCommandSignature();
};
