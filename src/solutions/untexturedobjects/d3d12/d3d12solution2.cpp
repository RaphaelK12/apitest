#include "pch.h"
#include "d3d12solution.h"
#include "framework/gfx_dx12.h"
#include "framework/gfx_dx11.h"	// for some utility function
#include <d3dcompiler.h>

extern comptr<ID3D12DescriptorHeap>	g_HeapRTV;
extern comptr<ID3D12DescriptorHeap>	g_HeapDSV;
extern comptr<ID3D12Resource>		g_BackBuffer;
extern comptr<ID3D12CommandQueue>	g_CommandQueue;
extern int	g_ClientWidth;
extern int	g_ClientHeight;
extern ID3D12Fence* g_FinishFence;
extern UINT64 g_finishFenceValue;
extern HANDLE g_finishFenceEvent;

UntexturedD3D12Solution::UntexturedD3D12Solution()
{
}

bool UntexturedD3D12Solution::Init(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
	const std::vector<UntexturedObjectsProblem::Index>& _indices,
	size_t _objectCount)
{
	if (!CreatePSO())
		return false;

	if (!CreateCommandAllocator())
		return false;

	UINT vertexSize = sizeof(UntexturedObjectsProblem::Vertex) * _vertices.size();
	m_IndexCount = _indices.size();
	UINT indexSize = sizeof(UntexturedObjectsProblem::Index) * m_IndexCount;
	UINT totalSize = vertexSize + indexSize;
	if (FAILED(g_D3D12Device->CreateCommittedResource(
		&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_MISC_NONE,
		&CD3D12_RESOURCE_DESC::Buffer(totalSize),
		D3D12_RESOURCE_USAGE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_UploadBuffer)
		)))
		return false;

	if (FAILED(g_D3D12Device->CreateCommittedResource(
		&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_MISC_NONE,
		&CD3D12_RESOURCE_DESC::Buffer(totalSize),
		D3D12_RESOURCE_USAGE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_DefaultBuffer)
		)))
		return false;
	m_VertexBufferView = { m_DefaultBuffer->GetGPUVirtualAddress(), vertexSize, sizeof(UntexturedObjectsProblem::Vertex) };
	m_IndexBufferView = { m_DefaultBuffer->GetGPUVirtualAddress() + vertexSize, indexSize, DXGI_FORMAT_R16_UINT };
	
	BYTE *pBuf = NULL;
	m_UploadBuffer->Map(0, NULL, (void**)&pBuf);
	memcpy(pBuf, &_vertices[0], vertexSize);
	memcpy(pBuf + vertexSize, &_indices[0],indexSize);
	m_UploadBuffer->Unmap(0, NULL);

	m_GraphicsCommandList->CopyBufferRegion(m_DefaultBuffer, 0, m_UploadBuffer, 0, totalSize, D3D12_COPY_NONE);
	
	AddResourceBarrier(m_GraphicsCommandList, m_DefaultBuffer, D3D12_RESOURCE_USAGE_COPY_DEST, D3D12_RESOURCE_USAGE_GENERIC_READ);
	m_GraphicsCommandList->Close();
	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_GraphicsCommandList);
	g_CommandQueue->Signal(g_FinishFence, ++g_finishFenceValue);
	g_FinishFence->SetEventOnCompletion(g_finishFenceValue, g_finishFenceEvent);
	WaitForSingleObject(g_finishFenceEvent, INFINITE);
	return true;
}

bool UntexturedD3D12Solution::CreatePSO()
{
	// compile shader first
	comptr<ID3DBlob> vsCode = CompileShader(L"cubes_d3d12_naive_vs.hlsl", "vsMain", "vs_5_0");
	comptr<ID3DBlob> psCode = CompileShader(L"cubes_d3d12_naive_ps.hlsl", "psMain", "ps_5_0");

	// Create Root signature
	D3D12_DESCRIPTOR_RANGE descRanges[2];
	descRanges[0].Init(D3D12_DESCRIPTOR_RANGE_CBV, 1, 0);
	descRanges[1].Init(D3D12_DESCRIPTOR_RANGE_CBV, 1, 1);

	D3D12_ROOT_PARAMETER rootParameters[2];
	//rootParameters[0].InitAsDescriptorTable(1, &descRanges[0], D3D12_SHADER_VISIBILITY_ALL);
	//rootParameters[1].InitAsDescriptorTable(1, &descRanges[1], D3D12_SHADER_VISIBILITY_ALL);

	rootParameters[0].InitAsConstants(16, 0);
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



bool UntexturedD3D12Solution::CreateCommandAllocator()
{
	// Create a command allocator
	if (FAILED(g_D3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&m_CommandAllocator))))
		return false;

	if (FAILED(g_D3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator, 0, __uuidof(ID3D12CommandList), (void**)&m_GraphicsCommandList)))
		return false;
	
	return true;
}


void UntexturedD3D12Solution::Render(const std::vector<Matrix>& _transforms)
{
	if (FAILED(m_CommandAllocator->Reset()))
		return;

	unsigned int total_count = _transforms.size();

	// Program
	Vec3 dir = { -0.5f, -1, 1 };
	Vec3 at = { 0, 0, 0 };
	Vec3 up = { 0, 0, 1 };
	dir = normalize(dir);
	Vec3 eye = at - 250.0f * dir;
	Matrix view = matrix_look_at(eye, at, up);
	int size = sizeof(MatrixBuffer);
	Matrix viewProj = mProj * view;

	// Reset command list
	m_GraphicsCommandList->Reset(m_CommandAllocator, m_PipelineState);

	// Set Render Target
	m_GraphicsCommandList->SetRenderTargets(&g_HeapRTV->GetCPUDescriptorHandleForHeapStart(), true, 1, &g_HeapDSV->GetCPUDescriptorHandleForHeapStart());

	// Setup root signature
	m_GraphicsCommandList->SetGraphicsRootSignature(m_RootSignature);

	// Setup viewport
	D3D12_VIEWPORT viewport = { 0, 0, FLOAT(g_ClientWidth), FLOAT(g_ClientHeight), 0.0f, 1.0f };
	m_GraphicsCommandList->RSSetViewports(1, &viewport);

	// Setup scissor
	D3D12_RECT scissorRect = { 0, 0, g_ClientWidth, g_ClientHeight };
	m_GraphicsCommandList->RSSetScissorRects(1, &scissorRect);
	

	// Setup pipeline state
	//m_GraphicsCommandList->SetPipelineState(m_PipelineState);

	// Draw the triangle
	m_GraphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_GraphicsCommandList->SetVertexBuffers(0, &m_VertexBufferView, 1);
	m_GraphicsCommandList->SetIndexBuffer(&m_IndexBufferView);

	m_GraphicsCommandList->SetGraphicsRoot32BitConstants(1, &viewProj, 0, 16);
	for (unsigned int u = 0; u < total_count; ++u) {
		// Setup constant buffer	
		m_GraphicsCommandList->SetGraphicsRoot32BitConstants(0, &_transforms[u], 0, 16);
		m_GraphicsCommandList->DrawIndexedInstanced(m_IndexCount, 1, 0, 0, 0);
	}


	// Close the command list
	m_GraphicsCommandList->Close();

	// Execute Command List
	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_GraphicsCommandList);
}

void UntexturedD3D12Solution::Shutdown()
{
	m_UploadBuffer.release();
	m_DefaultBuffer.release();

	m_PipelineState.release();
	m_RootSignature.release();


	m_CommandAllocator.release();
	m_BundleAllocator.release();
	m_CopyCommandAllocator.release();
	m_GraphicsCommandList.release();

}