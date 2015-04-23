#pragma once

#include "solutions/texturedquadssoln.h"

#define NUM_EXT_THREAD	8

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
class TexturedQuadsD3D12MultiThread : public TexturedQuadsSolution
{
public:
	TexturedQuadsD3D12MultiThread();
	virtual ~TexturedQuadsD3D12MultiThread() { }

	virtual bool Init(const std::vector<TexturedQuadsProblem::Vertex>& _vertices,
		const std::vector<TexturedQuadsProblem::Index>& _indices,
		const std::vector<TextureDetails*>& _textures,
		size_t _objectCount);

	virtual void Render(const std::vector<Matrix>& _transforms);
	virtual void Shutdown();

	virtual std::string GetName() const { return "D3D12MultiThread"; }
	virtual bool SupportsApi(EGfxApi _api) const override { return _api == EGfxApi::Direct3D12; }

private:
	struct ConstantsPerDraw
	{
		Matrix	World;
		UINT32	InstanceId;
	};

	struct ConstantsPerFrame
	{
		Matrix ViewProjection;
	};

	comptr<ID3D12PipelineState>			m_PipelineState;
	comptr<ID3D12RootSignature>			m_RootSignature;

	comptr<ID3D12Resource>				m_GeometryBuffer;
	D3D12_VERTEX_BUFFER_VIEW			m_VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW				m_IndexBufferView;

	HANDLE								m_ThreadHandle[NUM_EXT_THREAD];
	HANDLE								m_ThreadBeginEvent[NUM_EXT_THREAD];
	HANDLE								m_ThreadEndEvent[NUM_EXT_THREAD];
	comptr<ID3D12CommandAllocator>		m_CommandAllocator[NUM_ACCUMULATED_FRAMES][NUM_EXT_THREAD];
	comptr<ID3D12GraphicsCommandList>	m_CommandList[NUM_ACCUMULATED_FRAMES][NUM_EXT_THREAD];

	int									m_SRVDescriptorSize;
	int									m_SamplerDescriptorSize;

	std::vector<comptr<ID3D12Resource>>	m_Textures;
	comptr<ID3D12DescriptorHeap>		m_SRVHeap;
	comptr<ID3D12DescriptorHeap>		m_SamplerHeap;
	bool								m_ThreadEnded;
	
	Matrix								m_ViewProjection;
	const Matrix*						m_Transforms;
	size_t								m_ObjectCount;
	size_t								m_IndexCount;

	bool CreatePSO();
	bool CreateGeometryBuffer(const std::vector<TexturedQuadsProblem::Vertex>& _vertices,
		const std::vector<TexturedQuadsProblem::Index>& _indices);
	bool CreateCommandList();
	bool CreateTextures(const std::vector<TextureDetails*>& _textures);
	bool CreateThreads();
	void RenderPart(int pid, int total);
	void renderMultiThread(int tid);
};
