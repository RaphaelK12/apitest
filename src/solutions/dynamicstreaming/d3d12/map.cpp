#include "pch.h"

#include "problems/dynamicstreaming.h"
#include "map.h"
#include "framework/gfx_dx11.h"
#include "framework/gfx_dx12.h"

extern comptr<ID3D12DescriptorHeap>			g_HeapRTV;
extern comptr<ID3D12DescriptorHeap>			g_HeapDSV;
extern comptr<ID3D12Resource>				g_BackBuffer;
extern comptr<ID3D12CommandQueue>			g_CommandQueue;
extern int	g_ClientWidth;
extern int	g_ClientHeight;
extern int	g_curContext;

// Finish fence
extern comptr<ID3D12Fence> g_FinishFence;
extern UINT64 g_finishFenceValue;

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
DynamicStreamingD3D12Map::DynamicStreamingD3D12Map()
{
}
// --------------------------------------------------------------------------------------------------------------------
DynamicStreamingD3D12Map::~DynamicStreamingD3D12Map()
{
}

// --------------------------------------------------------------------------------------------------------------------
bool DynamicStreamingD3D12Map::Init(size_t _maxVertexCount)
{
	m_VertexData = 0;

	if (!CreatePSO())
		return false;

	if (!CreateCommandList())
		return false;

	if (!CreateGeometryBuffer(_maxVertexCount))
		return false;

	return true;
}

// --------------------------------------------------------------------------------------------------------------------
void DynamicStreamingD3D12Map::Render(const std::vector<Vec2>& _vertices)
{
	if (FAILED(m_CommandAllocator[g_curContext]->Reset()))
		return;

	Constants cb;
	cb.width = 2.0f / mWidth;
	cb.height = -2.0f / mHeight;
	
	// Update vertex buffer
	//memcpy(m_VertexData + mBufferSize * m_ContextId, _vertices.data(), mBufferSize);

	// Create Command List first time invoked
	{
		// Reset command list
		m_CommandList[g_curContext]->Reset(m_CommandAllocator[g_curContext], m_PipelineState);

		// Setup root signature
		m_CommandList[g_curContext]->SetGraphicsRootSignature(m_RootSignature);

		// Setup viewport
		D3D12_VIEWPORT viewport = { 0, 0, FLOAT(g_ClientWidth), FLOAT(g_ClientHeight), 0.0f, 1.0f };
		m_CommandList[g_curContext]->RSSetViewports(1, &viewport);

		// Setup scissor
		D3D12_RECT scissorRect = { 0, 0, g_ClientWidth, g_ClientHeight };
		m_CommandList[g_curContext]->RSSetScissorRects(1, &scissorRect);

		// Set Render Target
		m_CommandList[g_curContext]->OMSetRenderTargets(1,&g_HeapRTV->GetCPUDescriptorHandleForHeapStart(), true, &g_HeapDSV->GetCPUDescriptorHandleForHeapStart());

		// Draw the triangle
		m_CommandList[g_curContext]->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_CommandList[g_curContext]->IASetVertexBuffers(0, 1 , &m_VertexBufferView);

		// Set constant
		m_CommandList[g_curContext]->SetGraphicsRoot32BitConstants(0, 8, &cb, 0);

		// draw command
		size_t memOffset = 0;
		size_t vertOfset = 0;
		for (unsigned int u = 0; u < kParticleCount; ++u) {
			// could be done out of the loop, the memory copy is performed here in order to get apple to apple comparison
			memcpy(m_VertexData + kOffsetInBytes + memOffset, ((UINT8*)_vertices.data()) + memOffset, kPerticleInBytes);

			// issue draw command
			m_CommandList[g_curContext]->DrawInstanced(kVertsPerParticle, 1, kOffsetInVertices + vertOfset, 0);

			// update offset
			memOffset += kPerticleInBytes;
			vertOfset += kVertsPerParticle;
		}

		// close the command list
		m_CommandList[g_curContext]->Close();
	}

	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_CommandList[g_curContext]);
}

// --------------------------------------------------------------------------------------------------------------------
void DynamicStreamingD3D12Map::Shutdown()
{
	// Make sure everything is flushed out before releasing anything.
	HANDLE handleEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
	g_CommandQueue->Signal(g_FinishFence, ++g_finishFenceValue);
	g_FinishFence->SetEventOnCompletion(g_finishFenceValue, handleEvent);
	WaitForSingleObject(handleEvent, INFINITE);
	CloseHandle(handleEvent);

	m_RootSignature.release();
	m_PipelineState.release();
	m_GeometryBuffer.release();

	for (int k = 0; k < NUM_ACCUMULATED_FRAMES; k++)
	{
		m_CommandList[k].release();
		m_CommandAllocator[k].release();
	}
}

// --------------------------------------------------------------------------------------------------------------------
bool DynamicStreamingD3D12Map::CreatePSO()
{
	// compile shader first
	comptr<ID3DBlob> vsCode = CompileShader(L"streaming_vb_d3d12_vs.hlsl", "vsMain", "vs_5_0");
	comptr<ID3DBlob> psCode = CompileShader(L"streaming_vb_d3d12_ps.hlsl", "psMain", "ps_5_0");
	
	D3D12_ROOT_PARAMETER rootParameters[1];
	memset(&rootParameters[0], 0, sizeof(rootParameters[0]));
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[0].Constants.Num32BitValues = 8;

	D3D12_ROOT_SIGNATURE_DESC rootSig = { 1, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
	ID3DBlob* pBlobRootSig, *pBlobErrors;
	HRESULT hr = (D3D12SerializeRootSignature(&rootSig, D3D_ROOT_SIGNATURE_VERSION_1, &pBlobRootSig, &pBlobErrors));
	if (FAILED(hr))
		return false;

	hr = g_D3D12Device->CreateRootSignature(0, pBlobRootSig->GetBufferPointer(), pBlobRootSig->GetBufferSize(), __uuidof(ID3D12RootSignature), (void **)&m_RootSignature);
	if (FAILED(hr))
		return false;

	const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "Attr", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	const UINT numInputLayoutElements = sizeof(inputLayout) / sizeof(inputLayout[0]);

	// Create Pipeline State Object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psod;
	memset(&psod, 0, sizeof(psod));
	psod.pRootSignature = m_RootSignature;
	psod.VS.BytecodeLength = vsCode->GetBufferSize();
	psod.VS.pShaderBytecode = vsCode->GetBufferPointer();
	psod.PS.BytecodeLength = psCode->GetBufferSize();
	psod.PS.pShaderBytecode = psCode->GetBufferPointer();
	psod.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psod.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psod.RasterizerState.FrontCounterClockwise = true;
	psod.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psod.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psod.SampleDesc.Count = 1;
	psod.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psod.NumRenderTargets = 1;
	psod.SampleMask = UINT_MAX;
	psod.InputLayout = { inputLayout, numInputLayoutElements };
	psod.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	psod.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psod.DepthStencilState = CD3D12_DEPTH_STENCIL_DESC();

	return SUCCEEDED(g_D3D12Device->CreateGraphicsPipelineState(&psod, __uuidof(ID3D12PipelineState), (void**)&m_PipelineState));
}

// --------------------------------------------------------------------------------------------------------------------
bool DynamicStreamingD3D12Map::CreateCommandList()
{
	for (int k = 0; k < NUM_ACCUMULATED_FRAMES; k++)
	{
		// create command queue allocator
		HRESULT hr = g_D3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&m_CommandAllocator[k]);
		if (FAILED(hr))
			return false;

		// Create Command List
		g_D3D12Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator[k], 0, __uuidof(ID3D12GraphicsCommandList), (void**)&m_CommandList[k]);
		m_CommandList[k]->Close();
	}

	return true;
}

bool DynamicStreamingD3D12Map::CreateGeometryBuffer(size_t _maxVertexCount)
{
	const size_t sizeofVertex = sizeof(Vec2);
	m_BufferSize = sizeofVertex * _maxVertexCount;
	const size_t totalSize = m_BufferSize * NUM_ACCUMULATED_FRAMES;

	if (FAILED(g_D3D12Device->CreateCommittedResource(
		&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3D12_RESOURCE_DESC::Buffer(totalSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_GeometryBuffer)
		)))
		return false;

	m_VertexBufferView = D3D12_VERTEX_BUFFER_VIEW{ m_GeometryBuffer->GetGPUVirtualAddress(), totalSize, sizeofVertex };

	m_GeometryBuffer->Map(0, 0, reinterpret_cast<void**>(&m_VertexData));

	kVertexSizeBytes = sizeof(Vec2);
	kParticleCount = _maxVertexCount / kVertsPerParticle;
	kTotalVertices = _maxVertexCount;
	kOffsetInBytes = m_BufferSize * g_curContext;
	kOffsetInVertices = kTotalVertices * g_curContext;
	kPerticleInBytes = kVertsPerParticle * kVertexSizeBytes;
	
	return true;
}