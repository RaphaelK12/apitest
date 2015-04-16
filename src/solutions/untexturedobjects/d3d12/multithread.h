#pragma once

#include "solutions/untexturedobjectssoln.h"

#define NUM_EXT_THREAD	4

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
class UntexturedObjectsD3D12MultiThread : public UntexturedObjectsSolution
{
public:
	UntexturedObjectsD3D12MultiThread();
	virtual ~UntexturedObjectsD3D12MultiThread() { }

	virtual bool Init(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
		const std::vector<UntexturedObjectsProblem::Index>& _indices,
		size_t _objectCount) override;

	virtual void Render(const std::vector<Matrix>& _transforms);
	virtual void Shutdown();

	virtual std::string GetName() const { return "MultiThread"; }

	virtual bool SupportsApi(EGfxApi _api) const override { return _api == EGfxApi::Direct3D12; }

private:
	struct MatrixBuffer
	{
		Matrix m;
	};

	comptr<ID3D12PipelineState>			m_PipelineState[NUM_EXT_THREAD];
	comptr<ID3D12RootSignature>			m_RootSignature[NUM_EXT_THREAD];

	comptr<ID3D12Heap>					m_GeometryBufferHeap;
	comptr<ID3D12Resource>				m_VertexBuffer;
	comptr<ID3D12Resource>				m_IndexBuffer;
	D3D12_VERTEX_BUFFER_VIEW			m_VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW				m_IndexBufferView;

	comptr<ID3D12Resource>				m_ConstantBuffer;
	MatrixBuffer*						m_ConstantBufferData;

	Matrix								m_ViewProjection;
	Matrix*								m_Transforms;

	size_t								m_ObjectCount;
	size_t								m_IndexCount;
	size_t								m_DescriptorSize;

	HANDLE								m_ThreadHandle[NUM_EXT_THREAD];
	HANDLE								m_ThreadBeginEvent[NUM_EXT_THREAD];
	HANDLE								m_ThreadEndEvent[NUM_EXT_THREAD];
	comptr<ID3D12CommandAllocator>		m_CommandAllocator[NUM_EXT_THREAD];
	comptr<ID3D12GraphicsCommandList>	m_CommandList[NUM_EXT_THREAD];
	bool								m_ThreadEnded;

	bool CreatePSO();
	bool CreateGeometryBuffer(	const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
								const std::vector<UntexturedObjectsProblem::Index>& _indices);
	bool CreateConstantBuffer(size_t count);
	bool CreateThreads();
	bool CreateCommands();

	void RenderPart(int pid, int total);

	void renderMultiThread( int tid );
};
