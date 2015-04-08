#pragma once

#include "solutions/untexturedobjectssoln.h"

__declspec(align(16)) struct AlignedMatrix
{
	float data[16];

	void IdentityMatrix()
	{
		memset(data, 0, sizeof(data));
		data[0] = data[5] = data[10] = data[15] = 1.0f;
	}
	void FromMatrix( const Matrix& m )
	{
		data[0] = m.x.x; data[1] = m.x.y; data[2] = m.x.z; data[3] = m.x.w;
		data[4] = m.y.x; data[5] = m.y.y; data[6] = m.y.z; data[7] = m.y.w;
		data[8] = m.z.x; data[9] = m.z.y; data[10] = m.z.z; data[11] = m.z.w;
		data[12] = m.w.x; data[13] = m.w.y; data[14] = m.w.z; data[15] = m.w.w;
	}
};

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
	struct ConstantsBufferData
	{
		AlignedMatrix ViewProjection;
		AlignedMatrix World;
	};

	ID3D12Resource*				m_VertexBuffer;
	ID3D12Resource*				m_IndexBuffer;
	ID3D12Resource*				m_ConstantBuffer;
	ID3D12DescriptorHeap*		m_DescriptorHeap;
	D3D12_VERTEX_BUFFER_VIEW	m_VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW		m_IndexBufferView;

	ID3D12PipelineState*		m_PipelineState;
	ID3D12RootSignature*		m_RootSignature;

	ID3D12Heap*					m_BufferHeap;

	ID3D12CommandAllocator*		m_CommandAllocator;
	ID3D12GraphicsCommandList*	m_GraphicsCommandList;
	ConstantsBufferData*		m_ConstantBufferData;

	size_t						m_IndexCount;

// private method
	bool	CreatePSO();
	bool	CreateConstantBuffer();
	bool	CreateCommandAllocator();
};
