#include "pch.h"
#include "d3d12solution.h"
#include "framework/gfx_dx12.h"
#include "framework/gfx_dx11.h"	// for some utility function
#include <d3dcompiler.h>

extern comptr<ID3D12DescriptorHeap>	g_HeapRTV;
extern comptr<ID3D12DescriptorHeap>	g_HeapDSV;
extern comptr<ID3D12Resource>			g_BackBuffer;
extern comptr<ID3D12CommandQueue>		g_CommandQueue;
extern int	g_ClientWidth;
extern int	g_ClientHeight;

UntexturedD3D12Solution::UntexturedD3D12Solution():
m_WorldMatrixBufferData(),
m_ViewProjectionMatrixBufferData()
{
}

bool UntexturedD3D12Solution::Init(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
	const std::vector<UntexturedObjectsProblem::Index>& _indices,
	size_t _objectCount)
{
	const int size = 8 * 1024 * 1024;
	if (sizeof(UntexturedObjectsProblem::Vertex) * _vertices.size() + sizeof(UntexturedObjectsProblem::Index) * _indices.size() > size)
		return false;	// not enough memory allocated

	// Create copy command list
	if (!CreateCommandAllocator())
		return false;

	if (!CreatePSO())
		return false;

	if (!CreateConstantBuffer())
		return false;

	if (FAILED(g_D3D12Device->CreateHeap(
		&CD3D12_HEAP_DESC(size, D3D12_HEAP_TYPE_UPLOAD, 0, D3D12_HEAP_MISC_DENY_TEXTURES),
		__uuidof(ID3D12Heap),
		reinterpret_cast<void**>(&m_VertexBufferHeap))))
		return false;
	if (FAILED(g_D3D12Device->CreateHeap(
		&CD3D12_HEAP_DESC(size, D3D12_HEAP_TYPE_UPLOAD, 0, D3D12_HEAP_MISC_DENY_TEXTURES),
		__uuidof(ID3D12Heap),
		reinterpret_cast<void**>(&m_IndexBufferHeap))))
		return false;

	m_VertexBuffer = CreateBufferFromVector(_vertices, m_VertexBufferHeap, 0, m_CopyCommandList);
	m_IndexBuffer = CreateBufferFromVector(_indices, m_IndexBufferHeap, 0, m_CopyCommandList);
	if (!m_VertexBuffer || !m_IndexBuffer)
		return false;

	m_IndexCount = _indices.size();

	m_VertexBufferView = D3D12_VERTEX_BUFFER_VIEW{ m_VertexBuffer->GetGPUVirtualAddress(), sizeof(UntexturedObjectsProblem::Vertex) * _vertices.size(), sizeof(UntexturedObjectsProblem::Vertex) };
	m_IndexBufferView = D3D12_INDEX_BUFFER_VIEW{ m_IndexBuffer->GetGPUVirtualAddress(), sizeof(UntexturedObjectsProblem::Index) * _indices.size(), DXGI_FORMAT_R16_UINT };

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
	rootParameters[0].InitAsDescriptorTable(1, &descRanges[0], D3D12_SHADER_VISIBILITY_ALL);
	//rootParameters[0].InitAsConstants(32, 0);
	rootParameters[1].InitAsDescriptorTable(1, &descRanges[1], D3D12_SHADER_VISIBILITY_ALL);

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

bool UntexturedD3D12Solution::CreateConstantBuffer()
{
	const D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP, 262144 + 1 , D3D12_DESCRIPTOR_HEAP_SHADER_VISIBLE };

	if (FAILED(g_D3D12Device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&m_DescriptorHeap))))
		return false;

	// Create a (large) constant buffer
	if (FAILED(g_D3D12Device->CreateCommittedResource(
		&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_MISC_NONE,
		&CD3D12_RESOURCE_DESC::Buffer(sizeof(MatrixBuffer)),
		D3D12_RESOURCE_USAGE_GENERIC_READ,
		nullptr,
		__uuidof(ID3D12Resource),
		reinterpret_cast<void**>(&m_ViewProjectionBuffer))))
	{
		return false;
	}

	// Update constants
	m_ViewProjectionBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_ViewProjectionMatrixBufferData));
	
	// Create Descriptors
	D3D12_CONSTANT_BUFFER_VIEW_DESC cdesc = { m_ViewProjectionBuffer->GetGPUVirtualAddress(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT };
	D3D12_CPU_DESCRIPTOR_HANDLE offset_handle;

	offset_handle.ptr = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	g_D3D12Device->CreateConstantBufferView(&cdesc, offset_handle);

	return true;
}

bool UntexturedD3D12Solution::CreateCommandAllocator()
{
	// Create a command allocator
	if (FAILED(g_D3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&m_CommandAllocator))))
		return false;

	if (FAILED(g_D3D12Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator, 0, __uuidof(ID3D12CommandList), (void**)&m_GraphicsCommandList)))
		return false;
	m_GraphicsCommandList->Close();
	
	// Create a command allocator
	if (FAILED(g_D3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&m_BundleAllocator))))
		return false;

	if (FAILED(g_D3D12Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_BUNDLE, m_BundleAllocator, 0, __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void**>(&m_Bundle))))
		return false;
	m_Bundle->Close();

	// Create a command allocator
	if (FAILED(g_D3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&m_CopyCommandAllocator))))
		return false;

	if (FAILED(g_D3D12Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_BUNDLE, m_CopyCommandAllocator, 0, __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void**>(&m_CopyCommandList))))
		return false;
	m_CopyCommandList->Close();
	
	return true;
}

bool UntexturedD3D12Solution::RecordBundle(int count)
{
	static int descriptorSize = g_D3D12Device->GetDescriptorHandleIncrementSize(D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP);

	m_Bundle->Reset(m_BundleAllocator, 0);
	
	// Setup root signature
	m_Bundle->SetGraphicsRootSignature(m_RootSignature);

	// Setup descriptor heaps
	m_Bundle->SetDescriptorHeaps(&m_DescriptorHeap, 1);

	// Draw the triangle
	m_Bundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_Bundle->SetVertexBuffers(0, &m_VertexBufferView, 1);

	m_Bundle->SetPipelineState(m_PipelineState);
	
	D3D12_GPU_DESCRIPTOR_HANDLE offset_handle;
	offset_handle.ptr = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr;
	m_Bundle->SetGraphicsRootDescriptorTable(1, offset_handle);
	
	for (int u = 0; u < count; ++u) {
		// draw the instance
		offset_handle.ptr = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + descriptorSize * (u + 1);
		m_Bundle->SetGraphicsRootDescriptorTable(0, offset_handle);

		m_Bundle->DrawInstanced(m_IndexCount, 1, 0, 0);
	}
	m_Bundle->Close();

	return true;
}

void UntexturedD3D12Solution::Render(const std::vector<Matrix>& _transforms)
{
	if (FAILED(m_CommandAllocator->Reset()))
		return;

	static bool use_bundle = false;
	static int descriptorSize = g_D3D12Device->GetDescriptorHandleIncrementSize(D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP) ;
	static const UINT cbvIncrement = (sizeof(MatrixBuffer)+(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
	unsigned int total_count = _transforms.size();

	if (!m_WorldMatrixBufferData)
	{
		// Create a (large) constant buffer
		if (FAILED(g_D3D12Device->CreateCommittedResource(
			&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_MISC_NONE,
			&CD3D12_RESOURCE_DESC::Buffer(cbvIncrement*total_count),
			D3D12_RESOURCE_USAGE_GENERIC_READ,
			nullptr,
			__uuidof(ID3D12Resource),
			reinterpret_cast<void**>(&m_WorldMatrixBuffer))))
		{
			return;
		}
		/*
		// Create a (large) constant buffer
		if (FAILED(g_D3D12Device->CreateCommittedResource(
			&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_MISC_NONE,
			&CD3D12_RESOURCE_DESC::Buffer(cbvIncrement*total_count),
			D3D12_RESOURCE_USAGE_GENERIC_READ,
			nullptr,
			__uuidof(ID3D12Resource),
			reinterpret_cast<void**>(&m_WorldMatrixBufferGPU))))
		{
			return;
		}
		*/
		// Update constants
		m_WorldMatrixBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_WorldMatrixBufferData));

		for (unsigned int i = 0; i < total_count ; ++i)
		{
			// Create a constant buffer descriptor
			D3D12_CONSTANT_BUFFER_VIEW_DESC cdesc = { m_WorldMatrixBuffer->GetGPUVirtualAddress() + cbvIncrement * i, cbvIncrement };
			D3D12_CPU_DESCRIPTOR_HANDLE offset_handle;
			offset_handle.ptr = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + descriptorSize * (i + 1);
			g_D3D12Device->CreateConstantBufferView(&cdesc, offset_handle);
		}

		// record bundle
		if (use_bundle)
			RecordBundle(total_count);
	}

	// Program
	Vec3 dir = { -0.5f, -1, 1 };
	Vec3 at = { 0, 0, 0 };
	Vec3 up = { 0, 0, 1 };
	dir = normalize(dir);
	Vec3 eye = at - 250.0f * dir;
	Matrix view = matrix_look_at(eye, at, up);
	int size = sizeof(MatrixBuffer);
	for (unsigned int i = 0; i < (unsigned int)total_count; ++i)
		m_WorldMatrixBufferData[4 * i].m = _transforms[i];
	// setup viewprojection matrix
	m_ViewProjectionMatrixBufferData->m = mProj * view;
	/*
	{
		m_GraphicsCommandList->Reset(m_CommandAllocator, 0);
		m_GraphicsCommandList->CopyBufferRegion(m_WorldMatrixBufferGPU, 0, m_WorldMatrixBuffer, 0, cbvIncrement*total_count, D3D12_COPY_NONE);
		m_GraphicsCommandList->Close();
		// Execute Command List
		g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_GraphicsCommandList);
	}*/
	
	// Create Command List first time invoked
	if (!use_bundle)
	{
		// Reset command list
		m_GraphicsCommandList->Reset(m_CommandAllocator, 0);

		// Setup root signature
		m_GraphicsCommandList->SetGraphicsRootSignature(m_RootSignature);

		// Setup descriptor heaps
		m_GraphicsCommandList->SetDescriptorHeaps(&m_DescriptorHeap, 1);

		// Setup viewport
		D3D12_VIEWPORT viewport = { 0, 0, FLOAT(g_ClientWidth), FLOAT(g_ClientHeight), 0.0f, 1.0f };
		m_GraphicsCommandList->RSSetViewports(1, &viewport);

		// Setup scissor
		D3D12_RECT scissorRect = { 0, 0, g_ClientWidth, g_ClientHeight };
		m_GraphicsCommandList->RSSetScissorRects(1, &scissorRect);

		// Set Render Target
		m_GraphicsCommandList->SetRenderTargets(&g_HeapRTV->GetCPUDescriptorHandleForHeapStart(), true, 1, &g_HeapDSV->GetCPUDescriptorHandleForHeapStart());

		// Setup pipeline state
		m_GraphicsCommandList->SetPipelineState(m_PipelineState);

		// Draw the triangle
		m_GraphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_GraphicsCommandList->SetVertexBuffers(0, &m_VertexBufferView, 1);
		m_GraphicsCommandList->SetIndexBuffer(&m_IndexBufferView);

		D3D12_GPU_DESCRIPTOR_HANDLE offset_handle;
		offset_handle.ptr = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr;
		m_GraphicsCommandList->SetGraphicsRootDescriptorTable(1, offset_handle);
		
		for (unsigned int u = 0; u < total_count ; ++u) {
			// Setup constant buffer	
			offset_handle.ptr = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + descriptorSize * (u + 1);
			m_GraphicsCommandList->SetGraphicsRootDescriptorTable(0, offset_handle);
			//Matrix identity = matrix_identity();
			//m_GraphicsCommandList->SetGraphicsRoot32BitConstants(0, &identity, 0, 16);
			
			// draw the instance
			m_GraphicsCommandList->DrawIndexedInstanced(m_IndexCount, 1, 0, 0, 0);
		}
	}
	else
	{
		// Reset command list
		m_GraphicsCommandList->Reset(m_CommandAllocator, 0);

		// Setup root signature
		m_GraphicsCommandList->SetGraphicsRootSignature(m_RootSignature);

		// Setup descriptor heaps
		m_GraphicsCommandList->SetDescriptorHeaps(&m_DescriptorHeap, 1);

		// Setup viewport
		D3D12_VIEWPORT viewport = { 0, 0, FLOAT(g_ClientWidth), FLOAT(g_ClientHeight), 0.0f, 1.0f };
		m_GraphicsCommandList->RSSetViewports(1, &viewport);

		// Setup scissor
		D3D12_RECT scissorRect = { 0, 0, g_ClientWidth, g_ClientHeight };
		m_GraphicsCommandList->RSSetScissorRects(1, &scissorRect);

		// Set Render Target
		m_GraphicsCommandList->SetRenderTargets(&g_HeapRTV->GetCPUDescriptorHandleForHeapStart(), true, 1, &g_HeapDSV->GetCPUDescriptorHandleForHeapStart());

		// execute bundle
		m_GraphicsCommandList->ExecuteBundle(m_Bundle);
	}

	// Close the command list
	m_GraphicsCommandList->Close();

	// Execute Command List
	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_GraphicsCommandList);
}

void UntexturedD3D12Solution::Shutdown()
{
	m_WorldMatrixBufferData = 0;
	m_ViewProjectionMatrixBufferData = 0;

	m_VertexBuffer.release();
	m_IndexBuffer.release();
	m_WorldMatrixBuffer.release();
	m_ViewProjectionBuffer.release();
	m_DescriptorHeap.release();
	m_PipelineState.release();
	m_RootSignature.release();
	m_VertexBufferHeap.release();
	m_IndexBufferHeap.release();
	m_CommandAllocator.release();
	m_BundleAllocator.release();
	m_GraphicsCommandList.release();
	m_Bundle.release();
}