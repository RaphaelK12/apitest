#pragma once

#include "solutions/untexturedobjectssoln.h"

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
class UntexturedD3D12Solution : public UntexturedObjectsSolution
{
public:
	UntexturedD3D12Solution();
	virtual ~UntexturedD3D12Solution() { }

	virtual bool Init(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
		const std::vector<UntexturedObjectsProblem::Index>& _indices,
		size_t _objectCount) override;

	virtual void Render(const std::vector<Matrix>& _transforms);
	virtual void Shutdown();

	virtual std::string GetName() const { return "UntexturedD3D12Solution"; }

	virtual bool SupportsApi(EGfxApi _api) const override { return _api == EGfxApi::Direct3D12; }

private:
	struct MatrixBuffer
	{
		Matrix m;
	};

	comptr<ID3D12Resource>				m_VertexBuffer;
	comptr<ID3D12Resource>				m_IndexBuffer;
	comptr<ID3D12Resource>				m_WorldMatrixBuffer;
	comptr<ID3D12Resource>				m_ViewProjectionBuffer;
	comptr<ID3D12DescriptorHeap>		m_DescriptorHeap;
	comptr<ID3D12Resource>				m_UploadBuffer;
	comptr<ID3D12Resource>				m_DefaultBuffer;
	D3D12_VERTEX_BUFFER_VIEW			m_VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW				m_IndexBufferView;

	comptr<ID3D12PipelineState>			m_PipelineState;
	comptr<ID3D12RootSignature>			m_RootSignature;

	comptr<ID3D12Heap>					m_VertexBufferHeap;
	
	comptr<ID3D12CommandAllocator>		m_BundleAllocator;
	comptr<ID3D12GraphicsCommandList>	m_Bundle;
	MatrixBuffer*						m_WorldMatrixBufferData;
	MatrixBuffer*						m_ViewProjectionMatrixBufferData;

	size_t								m_IndexCount;
	size_t								m_DescriptorSize;

	comptr<ID3D12Resource>				m_CommandBuffer;
	comptr<ID3D12CommandSignature>		m_CommandSig;

	comptr<ID3D12CommandAllocator>		m_CommandAllocator;
	comptr<ID3D12GraphicsCommandList>	m_CommandList;

// private method
	bool	CreateGeometryBuffer(	const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
									const std::vector<UntexturedObjectsProblem::Index>& _indices);
	bool	CreatePSO();
	bool	CreateConstantBuffer(UINT total_count);
	bool	CreateCommandAllocator();
	bool	CreateCommandSignature();
	bool	CreateCommandList();
};
