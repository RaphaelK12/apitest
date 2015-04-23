#pragma once

#include "solutions/texturedquadssoln.h"

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
class TexturedQuadsD3D12Notex : public TexturedQuadsSolution
{
public:
	TexturedQuadsD3D12Notex();
	virtual ~TexturedQuadsD3D12Notex() { }

	virtual bool Init(const std::vector<TexturedQuadsProblem::Vertex>& _vertices,
		const std::vector<TexturedQuadsProblem::Index>& _indices,
		const std::vector<TextureDetails*>& _textures,
		size_t _objectCount);

	virtual void Render(const std::vector<Matrix>& _transforms);
	virtual void Shutdown();

	virtual std::string GetName() const { return "D3D12Notex (SingleThread SOL)"; }
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

	comptr<ID3D12CommandAllocator>		m_CommandAllocator[NUM_ACCUMULATED_FRAMES];
	comptr<ID3D12GraphicsCommandList>	m_CommandList[NUM_ACCUMULATED_FRAMES];

	size_t								m_IndexCount;

	bool CreatePSO();
	bool CreateGeometryBuffer(	const std::vector<TexturedQuadsProblem::Vertex>& _vertices,
								const std::vector<TexturedQuadsProblem::Index>& _indices);
	bool CreateCommandList();
};
