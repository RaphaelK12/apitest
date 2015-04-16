#pragma once

#include "solutions/untexturedobjectssoln.h"

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
class UntexturedObjectsD3D12SetConstants : public UntexturedObjectsSolution
{
public:
	UntexturedObjectsD3D12SetConstants();
	virtual ~UntexturedObjectsD3D12SetConstants() { }

	virtual bool Init(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
		const std::vector<UntexturedObjectsProblem::Index>& _indices,
		size_t _objectCount) override;

	virtual void Render(const std::vector<Matrix>& _transforms);
	virtual void Shutdown();

	virtual std::string GetName() const { return "UntexturedSetConstants"; }

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

	size_t								m_IndexCount;
	size_t								m_DescriptorSize;

	comptr<ID3D12CommandAllocator>		m_CommandAllocator;
	comptr<ID3D12GraphicsCommandList>	m_CommandList;

	bool CreatePSO();
	bool CreateGeometryBuffer(	const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
								const std::vector<UntexturedObjectsProblem::Index>& _indices);
	bool CreateCommandList();
};
