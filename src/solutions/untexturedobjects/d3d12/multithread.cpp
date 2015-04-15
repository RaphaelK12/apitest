#include "pch.h"
#include "multithread.h"
#include "framework/gfx_dx12.h"
#include "framework/gfx_dx11.h"	// for some utility function
#include <d3dcompiler.h>

extern comptr<ID3D12DescriptorHeap>			g_HeapRTV;
extern comptr<ID3D12DescriptorHeap>			g_HeapDSV;
extern comptr<ID3D12Resource>				g_BackBuffer;
extern comptr<ID3D12CommandQueue>			g_CommandQueue;
extern comptr<ID3D12GraphicsCommandList>	g_CommandList;
extern int	g_ClientWidth;
extern int	g_ClientHeight;

// Finish fence
extern comptr<ID3D12Fence> g_FinishFence;
extern UINT64 g_finishFenceValue;
extern HANDLE g_finishFenceEvent;

UntexturedObjectsD3D12MultiThread::UntexturedObjectsD3D12MultiThread()
{
}

bool UntexturedObjectsD3D12MultiThread::Init(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
	const std::vector<UntexturedObjectsProblem::Index>& _indices,
	size_t _objectCount)
{
	if (!CreatePSO())
		return false;

	if (!CreateGeometryBuffer(_vertices, _indices))
		return false;

	if (!CreateConstantBuffer(_objectCount))
		return false;

	return true;
}

bool UntexturedObjectsD3D12MultiThread::CreatePSO()
{
	// compile shader first
	comptr<ID3DBlob> vsCode = CompileShader(L"cubes_d3d12_naive_vs.hlsl", "vsMain", "vs_5_0");
	comptr<ID3DBlob> psCode = CompileShader(L"cubes_d3d12_naive_ps.hlsl", "psMain", "ps_5_0");

	D3D12_ROOT_PARAMETER rootParameters[2];
	rootParameters[0].InitAsConstantBufferView(0);
	rootParameters[1].InitAsConstants(16, 1);

	D3D12_ROOT_SIGNATURE rootSig = { 2, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
	ID3DBlob* pBlobRootSig, *pBlobErrors;
	HRESULT hr = (D3D12SerializeRootSignature(&rootSig, D3D_ROOT_SIGNATURE_V1, &pBlobRootSig, &pBlobErrors));
	if (FAILED(hr))
		return false;

	hr = g_D3D12Device->CreateRootSignature(0, pBlobRootSig->GetBufferPointer(), pBlobRootSig->GetBufferSize(), __uuidof(ID3D12RootSignature), (void **)&m_RootSignature);
	if (FAILED(hr))
		return false;

	const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "ObjPos", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_PER_VERTEX_DATA, 0 },
		{ "Color", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_PER_VERTEX_DATA, 0 },
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
	psod.RasterizerState.FillMode = D3D12_FILL_SOLID;
	psod.RasterizerState.CullMode = D3D12_CULL_NONE;
	psod.RasterizerState.FrontCounterClockwise = true;
	psod.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psod.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psod.SampleDesc.Count = 1;
	psod.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psod.NumRenderTargets = 1;
	psod.SampleMask = UINT_MAX;
	psod.InputLayout = { inputLayout, numInputLayoutElements };
	psod.IndexBufferProperties = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	psod.DepthStencilState = CD3D12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

	return SUCCEEDED(g_D3D12Device->CreateGraphicsPipelineState(&psod, __uuidof(ID3D12PipelineState), (void**)&m_PipelineState));
}

bool UntexturedObjectsD3D12MultiThread::CreateGeometryBuffer(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
																const std::vector<UntexturedObjectsProblem::Index>& _indices)
{
	const size_t size = 65536 * 4;

	if (FAILED(g_D3D12Device->CreateHeap(
		&CD3D12_HEAP_DESC(size, D3D12_HEAP_TYPE_UPLOAD, 0, D3D12_HEAP_MISC_DENY_TEXTURES),
		__uuidof(ID3D12Heap),
		reinterpret_cast<void**>(&m_GeometryBufferHeap))))
		return false;

	m_VertexBuffer = CreateBufferFromVector(_vertices, m_GeometryBufferHeap, 0);
	m_IndexBuffer = CreateBufferFromVector(_indices, m_GeometryBufferHeap, 0x10000);	// Minimum size for VB and IB is 64K ?
	if (!m_VertexBuffer || !m_IndexBuffer)
		return false;

	m_IndexCount = _indices.size();

	m_VertexBufferView = D3D12_VERTEX_BUFFER_VIEW{ m_VertexBuffer->GetGPUVirtualAddress(), sizeof(UntexturedObjectsProblem::Vertex) * _vertices.size(), sizeof(UntexturedObjectsProblem::Vertex) };
	m_IndexBufferView = D3D12_INDEX_BUFFER_VIEW{ m_IndexBuffer->GetGPUVirtualAddress(), sizeof(UntexturedObjectsProblem::Index) * _indices.size(), DXGI_FORMAT_R16_UINT };

	return true;
}

bool UntexturedObjectsD3D12MultiThread::CreateConstantBuffer(size_t count)
{
	// Create a (large) constant buffer
	if (FAILED(g_D3D12Device->CreateCommittedResource(
		&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_MISC_NONE,
		&CD3D12_RESOURCE_DESC::Buffer(count * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT),
		D3D12_RESOURCE_USAGE_GENERIC_READ,
		nullptr,
		__uuidof(ID3D12Resource),
		reinterpret_cast<void**>(&m_ConstantBuffer))))
	{
		return false;
	}

	m_ConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_ConstantBufferData));

	return true;
}

void UntexturedObjectsD3D12MultiThread::Render(const std::vector<Matrix>& _transforms)
{
	unsigned int count = _transforms.size();

	// Program
	Vec3 dir = { -0.5f, -1, 1 };
	Vec3 at = { 0, 0, 0 };
	Vec3 up = { 0, 0, 1 };
	dir = normalize(dir);
	Vec3 eye = at - 250.0f * dir;
	for (unsigned int i = 0; i < (unsigned int)count; ++i)
		m_ConstantBufferData[4 * i].m = _transforms[i];
	Matrix vp = mProj * matrix_look_at(eye, at, up);

	// Create Command List first time invoked
	{
		// Setup root signature
		g_CommandList->SetGraphicsRootSignature(m_RootSignature);

		// Setup viewport
		D3D12_VIEWPORT viewport = { 0, 0, FLOAT(g_ClientWidth), FLOAT(g_ClientHeight), 0.0f, 1.0f };
		g_CommandList->RSSetViewports(1, &viewport);

		// Setup scissor
		D3D12_RECT scissorRect = { 0, 0, g_ClientWidth, g_ClientHeight };
		g_CommandList->RSSetScissorRects(1, &scissorRect);

		// Set Render Target
		g_CommandList->SetRenderTargets(&g_HeapRTV->GetCPUDescriptorHandleForHeapStart(), true, 1, &g_HeapDSV->GetCPUDescriptorHandleForHeapStart());

		// Setup pipeline state
		g_CommandList->SetPipelineState(m_PipelineState);

		// Draw the triangle
		g_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		g_CommandList->SetVertexBuffers(0, &m_VertexBufferView, 1);
		g_CommandList->SetIndexBuffer(&m_IndexBufferView);

		g_CommandList->SetGraphicsRoot32BitConstants(1, &vp, 0, 16);

		unsigned int counter = 0;
		for (unsigned int u = 0; u < count ; ++u) {
			g_CommandList->SetGraphicsRootConstantBufferView(0, m_ConstantBuffer->GetGPUVirtualAddress() + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT * u);
			g_CommandList->DrawIndexedInstanced(m_IndexCount, 1, 0, 0, 0);
		}
	}
}

void UntexturedObjectsD3D12MultiThread::Shutdown()
{
	// Make sure everything is flushed out before releasing anything.
	g_CommandQueue->Signal(g_FinishFence, ++g_finishFenceValue);
	g_FinishFence->SetEventOnCompletion(g_finishFenceValue, g_finishFenceEvent);
	WaitForSingleObject(g_finishFenceEvent, INFINITE);

	m_RootSignature.release();
	m_PipelineState.release();

	m_VertexBuffer.release();
	m_IndexBuffer.release();
	m_GeometryBufferHeap.release();

	m_ConstantBuffer.release();
}