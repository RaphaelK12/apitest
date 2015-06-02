#include "pch.h"
#include "setconstantbufferview.h"
#include "framework/gfx_dx12.h"
#include "framework/gfx_dx11.h"	// for some utility function
#include <d3dcompiler.h>

extern comptr<ID3D12CommandQueue>			g_CommandQueue;
extern int	g_ClientWidth;
extern int	g_ClientHeight;
extern int	g_curContext;

// Finish fence
extern comptr<ID3D12Fence> g_FinishFence;
extern UINT64 g_finishFenceValue;

UntexturedObjectsD3D12SetConstantBufferView::UntexturedObjectsD3D12SetConstantBufferView()
{
}

bool UntexturedObjectsD3D12SetConstantBufferView::Init(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
	const std::vector<UntexturedObjectsProblem::Index>& _indices,
	size_t _objectCount)
{
	if (!CreatePSO())
		return false;

	if (!CreateGeometryBuffer(_vertices, _indices))
		return false;

	if (!CreateConstantBuffer(_objectCount))
		return false;

	if (!CreateCommandList())
		return false;

	return true;
}

bool UntexturedObjectsD3D12SetConstantBufferView::CreatePSO()
{
	// compile shader first
	comptr<ID3DBlob> vsCode = CompileShader(L"cubes_d3d12_naive_vs.hlsl", "vsMain", "vs_5_0");
	comptr<ID3DBlob> psCode = CompileShader(L"cubes_d3d12_naive_ps.hlsl", "psMain", "ps_5_0");

	D3D12_ROOT_PARAMETER rootParameters[2];
	memset(rootParameters, 0, sizeof(rootParameters));
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[1].Constants.Num32BitValues = 16;
	rootParameters[1].Constants.ShaderRegister = 1;

	D3D12_ROOT_SIGNATURE_DESC rootSig = { 2, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
	ID3DBlob* pBlobRootSig, *pBlobErrors;
	HRESULT hr = (D3D12SerializeRootSignature(&rootSig, D3D_ROOT_SIGNATURE_VERSION_1, &pBlobRootSig, &pBlobErrors));
	if (FAILED(hr))
		return false;

	hr = g_D3D12Device->CreateRootSignature(0, pBlobRootSig->GetBufferPointer(), pBlobRootSig->GetBufferSize(), __uuidof(ID3D12RootSignature), (void **)&m_RootSignature);
	if (FAILED(hr))
		return false;

	const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "ObjPos", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "Color", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
	psod.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	psod.DepthStencilState.DepthEnable = true;
	psod.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

	return SUCCEEDED(g_D3D12Device->CreateGraphicsPipelineState(&psod, __uuidof(ID3D12PipelineState), (void**)&m_PipelineState));
}

bool UntexturedObjectsD3D12SetConstantBufferView::CreateGeometryBuffer(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
																const std::vector<UntexturedObjectsProblem::Index>& _indices)
{
	m_IndexCount = _indices.size();

	const size_t sizeofVertex = sizeof(UntexturedObjectsProblem::Vertex);
	const size_t sizeofVertices = sizeofVertex * _vertices.size();
	const size_t sizeofIndex = sizeof(UntexturedObjectsProblem::Index);
	const size_t sizeofIndices = sizeofIndex * _indices.size();
	const size_t totalSize = sizeofIndices + sizeofIndices;

	if (FAILED(g_D3D12Device->CreateCommittedResource(
		&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3D12_RESOURCE_DESC::Buffer(totalSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_GeometryBuffer)
		)))
		return false;

	UINT8* pRaw = 0;
	m_GeometryBuffer->Map(0, 0, reinterpret_cast<void**>(&pRaw));
	memcpy(pRaw, _vertices.data(), sizeofVertices);
	memcpy(pRaw + sizeofVertices, _indices.data(), sizeofIndices);
	m_GeometryBuffer->Unmap(0, 0);

	m_IndexCount = _indices.size();

	m_VertexBufferView = D3D12_VERTEX_BUFFER_VIEW{ m_GeometryBuffer->GetGPUVirtualAddress(), sizeofVertices, sizeofVertex };
	m_IndexBufferView = D3D12_INDEX_BUFFER_VIEW{ m_GeometryBuffer->GetGPUVirtualAddress() + sizeofVertices, sizeofIndices, DXGI_FORMAT_R16_UINT };
	
	return true;
}

bool UntexturedObjectsD3D12SetConstantBufferView::CreateConstantBuffer(size_t count)
{
	// Create a (large) constant buffer
	for (int k = 0; k < NUM_ACCUMULATED_FRAMES; k++)
	{
		if (FAILED(g_D3D12Device->CreateCommittedResource(
			&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3D12_RESOURCE_DESC::Buffer(count * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			__uuidof(ID3D12Resource),
			reinterpret_cast<void**>(&m_ConstantBuffer[k]))))
		{
			return false;
		}
	}
	
	return true;
}

void UntexturedObjectsD3D12SetConstantBufferView::Render(const std::vector<Matrix>& _transforms)
{
	if (FAILED(m_CommandAllocator[g_curContext]->Reset()))
		return;
	
	unsigned int count = _transforms.size();

	// Program
	Vec3 dir = { -0.5f, -1, 1 };
	Vec3 at = { 0, 0, 0 };
	Vec3 up = { 0, 0, 1 };
	dir = normalize(dir);
	Vec3 eye = at - 250.0f * dir;

	m_ConstantBuffer[g_curContext]->Map(0, nullptr, reinterpret_cast<void**>(&m_ConstantBufferData));
	for (unsigned int i = 0; i < (unsigned int)count; ++i)
		m_ConstantBufferData[4 * i].m = _transforms[i];
	Matrix vp = mProj * matrix_look_at(eye, at, up);

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
		m_CommandList[g_curContext]->OMSetRenderTargets(1, &GetRenderTargetHandle(), true, &GetDepthStencialHandle());

		// Draw the triangle
		m_CommandList[g_curContext]->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_CommandList[g_curContext]->IASetVertexBuffers(0, 1, &m_VertexBufferView);
		m_CommandList[g_curContext]->IASetIndexBuffer(&m_IndexBufferView);

		m_CommandList[g_curContext]->SetGraphicsRoot32BitConstants(1, 16, &vp, 0);

		unsigned int counter = 0;
		for (unsigned int u = 0; u < count ; ++u) {
			m_CommandList[g_curContext]->SetGraphicsRootConstantBufferView(0, m_ConstantBuffer[g_curContext]->GetGPUVirtualAddress() + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT * u);
			m_CommandList[g_curContext]->DrawIndexedInstanced(m_IndexCount, 1, 0, 0, 0);
		}
		m_CommandList[g_curContext]->Close();
	}

	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_CommandList[g_curContext]);
}

void UntexturedObjectsD3D12SetConstantBufferView::Shutdown()
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
		m_ConstantBuffer[k].release();
		m_CommandAllocator[k].release();
		m_CommandList[k].release();
	}

	m_ConstantBufferData = 0;
}

bool UntexturedObjectsD3D12SetConstantBufferView::CreateCommandList()
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