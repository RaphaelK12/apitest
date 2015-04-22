#pragma once

#include "solutions/dynamicstreamingsoln.h"

#define NUM_EXT_THREAD	8

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
	struct Constants
	{
		float width;
		float height;
	};

	comptr<ID3D12PipelineState>			m_PipelineState;
	comptr<ID3D12RootSignature>			m_RootSignature;

	comptr<ID3D12Resource>				m_GeometryBuffer;
	D3D12_VERTEX_BUFFER_VIEW			m_VertexBufferView;

	comptr<ID3D12CommandAllocator>		m_CommandAllocator[NUM_ACCUMULATED_FRAMES][NUM_EXT_THREAD];
	comptr<ID3D12GraphicsCommandList>	m_CommandList[NUM_ACCUMULATED_FRAMES][NUM_EXT_THREAD];
	HANDLE								m_ThreadHandle[NUM_EXT_THREAD];
	HANDLE								m_ThreadBeginEvent[NUM_EXT_THREAD];
	HANDLE								m_ThreadEndEvent[NUM_EXT_THREAD];
	bool								m_ThreadEnded;

	UINT64								m_curFenceValue[NUM_ACCUMULATED_FRAMES];
	int									m_ContextId;
	UINT8*								m_VertexData;
	
	size_t								m_BufferSize;
	size_t								kVertexSizeBytes;
	size_t								kParticleCount;
	size_t								kTotalVertices;
	size_t								kOffsetInBytes;
	size_t								kOffsetInVertices;
	size_t								kPerticleInBytes;

	Constants							m_ConstantData;
	UINT8*								m_VertexSource;

	bool CreatePSO();
	bool CreateGeometryBuffer(size_t _maxVertexCount);
	bool CreateCommandList();
	bool CreateThreads();

	void RenderPart(int pid, int total);
	void renderMultiThread(int tid);
};
