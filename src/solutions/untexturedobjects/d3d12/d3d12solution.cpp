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

extern ID3D12Fence* g_FinishFence;
extern UINT64 g_finishFenceValue;
extern HANDLE g_finishFenceEvent;

static bool g_UseBundle = false;

static size_t g_SegmentCount = 1;

#define	USE_CONSTANTS	0
#define	USE_INDIRECT	1
#define USE_BUNDLE		1

UntexturedD3D12Solution::UntexturedD3D12Solution():
m_WorldMatrixBufferData(),
m_ViewProjectionMatrixBufferData()
{
}

bool UntexturedD3D12Solution::Init(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
	const std::vector<UntexturedObjectsProblem::Index>& _indices,
	size_t _objectCount)
{
	m_DescriptorSize = g_D3D12Device->GetDescriptorHandleIncrementSize(D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP);

	// Create copy command list
	if (!CreateCommandAllocator())
		return false;

	if (!CreatePSO())
		return false;

	if (!CreateGeometryBuffer(_vertices, _indices))
		return false;

	if (!CreateCommandSignature())
		return false;

	if (!CreateConstantBuffer(_objectCount))
		return false;

	return true;
}

bool UntexturedD3D12Solution::CreateGeometryBuffer(	const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
													const std::vector<UntexturedObjectsProblem::Index>& _indices)
{
	const size_t size = 65536 * 4;

	if (FAILED(g_D3D12Device->CreateHeap(
		&CD3D12_HEAP_DESC(size, D3D12_HEAP_TYPE_UPLOAD, 0, D3D12_HEAP_MISC_DENY_TEXTURES),
		__uuidof(ID3D12Heap),
		reinterpret_cast<void**>(&m_VertexBufferHeap))))
		return false;

	m_VertexBuffer = CreateBufferFromVector(_vertices, m_VertexBufferHeap, 0);
	m_IndexBuffer = CreateBufferFromVector(_indices, m_VertexBufferHeap, 0x10000);	// Minimum size for VB and IB is 64K ?
	if (!m_VertexBuffer || !m_IndexBuffer)
		return false;

	m_IndexCount = _indices.size();

	m_VertexBufferView = D3D12_VERTEX_BUFFER_VIEW{ m_VertexBuffer->GetGPUVirtualAddress(), sizeof(UntexturedObjectsProblem::Vertex) * _vertices.size(), sizeof(UntexturedObjectsProblem::Vertex) };
	m_IndexBufferView = D3D12_INDEX_BUFFER_VIEW{ m_IndexBuffer->GetGPUVirtualAddress(), sizeof(UntexturedObjectsProblem::Index) * _indices.size(), DXGI_FORMAT_R16_UINT };

#if USE_INDIRECT
	std::vector<D3D12_DRAW_INDEXED_ARGUMENTS> args;
	D3D12_DRAW_INDEXED_ARGUMENTS arg;
	arg.BaseVertexLocation = 0;
	arg.IndexCountPerInstance = 36;
	arg.InstanceCount = 1;
	arg.StartIndexLocation = 0;
	arg.StartInstanceLocation = 0;
	args.resize(1024);
	for (int i = 0; i < 1024; ++i)
		args[i] = arg;

	m_CommandBuffer = CreateBufferFromVector(args, m_VertexBufferHeap, 0x20000);
	if (!m_CommandBuffer)
		return false;
#endif
	/*
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
	memcpy(pBuf + vertexSize, &_indices[0], indexSize);
	m_UploadBuffer->Unmap(0, NULL);

	m_GraphicsCommandList->CopyBufferRegion(m_DefaultBuffer, 0, m_UploadBuffer, 0, totalSize, D3D12_COPY_NONE);

	AddResourceBarrier(m_GraphicsCommandList, m_DefaultBuffer, D3D12_RESOURCE_USAGE_COPY_DEST, D3D12_RESOURCE_USAGE_GENERIC_READ);
	m_GraphicsCommandList->Close();
	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_GraphicsCommandList);
	g_CommandQueue->Signal(g_FinishFence, ++g_finishFenceValue);
	g_FinishFence->SetEventOnCompletion(g_finishFenceValue, g_finishFenceEvent);
	WaitForSingleObject(g_finishFenceEvent, INFINITE);
	*/
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
#if USE_CONSTANTS
	rootParameters[0].InitAsConstants(16, 0);
	rootParameters[1].InitAsConstants(16, 1);
#else
	rootParameters[0].InitAsDescriptorTable(1, &descRanges[0], D3D12_SHADER_VISIBILITY_ALL);
	rootParameters[1].InitAsDescriptorTable(1, &descRanges[1], D3D12_SHADER_VISIBILITY_ALL);
#endif

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

bool UntexturedD3D12Solution::CreateConstantBuffer(UINT total_count)
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
	
	static const UINT cbvIncrement = (sizeof(MatrixBuffer)*g_SegmentCount+(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);

	// Create Descriptors
	D3D12_CONSTANT_BUFFER_VIEW_DESC cdesc = { m_ViewProjectionBuffer->GetGPUVirtualAddress(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT };
	D3D12_CPU_DESCRIPTOR_HANDLE offset_handle;
	offset_handle.ptr = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	g_D3D12Device->CreateConstantBufferView(&cdesc, offset_handle);

	// Create a (large) constant buffer
	if (FAILED(g_D3D12Device->CreateCommittedResource(
		&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_MISC_NONE,
		&CD3D12_RESOURCE_DESC::Buffer(total_count * (cbvIncrement / g_SegmentCount)),
		D3D12_RESOURCE_USAGE_GENERIC_READ,
		nullptr,
		__uuidof(ID3D12Resource),
		reinterpret_cast<void**>(&m_WorldMatrixBuffer))))
	{
		return false;
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

	for (unsigned int i = 0; i < total_count / g_SegmentCount; ++i)
	{
		// Create a constant buffer descriptor
		D3D12_CONSTANT_BUFFER_VIEW_DESC cdesc = { m_WorldMatrixBuffer->GetGPUVirtualAddress() + cbvIncrement * i , cbvIncrement };
		D3D12_CPU_DESCRIPTOR_HANDLE offset_handle;
		offset_handle.ptr = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + m_DescriptorSize * (i + 1);
		g_D3D12Device->CreateConstantBufferView(&cdesc, offset_handle);
	}

	// record bundle
#if USE_BUNDLE
	RecordBundle(total_count);
#endif

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

bool UntexturedD3D12Solution::CreateCommandSignature()
{
	D3D12_INDIRECT_PARAMETER para[1];
	para[0].Type = D3D12_INDIRECT_PARAMETER_DRAW_INDEXED;

	D3D12_COMMAND_SIGNATURE desc;
	desc.NodeMask = 1;
	desc.ParameterCount = 1;
	desc.pParameters = para;
	desc.ByteStride = 40;

	if (FAILED(g_D3D12Device->CreateCommandSignature(&desc, 0, __uuidof(ID3D12CommandSignature), reinterpret_cast<void**>(&m_CommandSig))))
		return false;

	return true;
}

bool UntexturedD3D12Solution::RecordBundle(int count)
{
	m_Bundle->Reset(m_BundleAllocator, 0);
	
	// Setup root signature
	m_Bundle->SetGraphicsRootSignature(m_RootSignature);

	// Setup descriptor heaps
	m_Bundle->SetDescriptorHeaps(&m_DescriptorHeap, 1);

	// Draw the triangle
	m_Bundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_Bundle->SetVertexBuffers(0, &m_VertexBufferView, 1);
	m_Bundle->SetIndexBuffer(&m_IndexBufferView);

	m_Bundle->SetPipelineState(m_PipelineState);
	

	#if USE_CONSTANTS
		//m_Bundle->SetGraphicsRoot32BitConstants(1, &vp, 0, 16);
	#else
		D3D12_GPU_DESCRIPTOR_HANDLE offset_handle;
		offset_handle.ptr = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr;
		m_Bundle->SetGraphicsRootDescriptorTable(1, offset_handle);
	#endif

	unsigned int counter = 0;
	for (unsigned int u = 0; u < count / g_SegmentCount; ++u) {
		#if USE_CONSTANTS
			//m_Bundle->SetGraphicsRoot32BitConstants(0, &_transforms[u], 0, 16);
		#else
			// Setup constant buffer
			offset_handle.ptr = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + m_DescriptorSize * (u + 1);
			m_Bundle->SetGraphicsRootDescriptorTable(0, offset_handle);
		#endif

		// draw the instance
		#if USE_INDIRECT
			m_Bundle->ExecuteIndirect(m_CommandSig, g_SegmentCount, m_CommandBuffer, 0, 0, 0);
		#else
			m_Bundle->DrawIndexedInstanced(m_IndexCount, g_SegmentCount, 0, 0, 0);
		#endif
	}
	m_Bundle->Close();

	return true;
}

void UntexturedD3D12Solution::Render(const std::vector<Matrix>& _transforms)
{
	if (FAILED(m_CommandAllocator->Reset()))
		return;

	unsigned int count = _transforms.size();

	// Program
	Vec3 dir = { -0.5f, -1, 1 };
	Vec3 at = { 0, 0, 0 };
	Vec3 up = { 0, 0, 1 };
	dir = normalize(dir);
	Vec3 eye = at - 250.0f * dir;
	Matrix view = matrix_look_at(eye, at, up);
	for (unsigned int i = 0; i < (unsigned int)count; ++i)
		m_WorldMatrixBufferData[4*i].m = _transforms[i];
	// setup viewprojection matrix
	Matrix vp = mProj * view;
	m_ViewProjectionMatrixBufferData->m = vp;
	/*
	{
		m_GraphicsCommandList->Reset(m_CommandAllocator, 0);
		m_GraphicsCommandList->CopyBufferRegion(m_WorldMatrixBufferGPU, 0, m_WorldMatrixBuffer, 0, cbvIncrement*total_count, D3D12_COPY_NONE);
		m_GraphicsCommandList->Close();
		// Execute Command List
		g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_GraphicsCommandList);
	}*/
	
	// Create Command List first time invoked
#if !USE_BUNDLE
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

#if USE_CONSTANTS
		m_GraphicsCommandList->SetGraphicsRoot32BitConstants(1, &vp, 0, 16);
#else
		D3D12_GPU_DESCRIPTOR_HANDLE offset_handle;
		offset_handle.ptr = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr;
		m_GraphicsCommandList->SetGraphicsRootDescriptorTable(1, offset_handle);
#endif
	
		unsigned int counter = 0;
		for (unsigned int u = 0; u < count / g_SegmentCount ; ++u) {
#if USE_CONSTANTS
			m_GraphicsCommandList->SetGraphicsRoot32BitConstants(0, &_transforms[u], 0, 16);
#else
			// Setup constant buffer
			offset_handle.ptr = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + m_DescriptorSize * (u + 1);
			m_GraphicsCommandList->SetGraphicsRootDescriptorTable(0, offset_handle);
#endif
			//Matrix identity = matrix_identity();
			//m_GraphicsCommandList->SetGraphicsRoot32BitConstants(0, &identity, 0, 16);
			
			// draw the instance
#if USE_INDIRECT
			m_GraphicsCommandList->ExecuteIndirect(m_CommandSig, g_SegmentCount, m_CommandBuffer, 0, 0, 0);
#else
			m_GraphicsCommandList->DrawIndexedInstanced(m_IndexCount, g_SegmentCount, 0, 0, 0);
#endif
		}
	}
#else
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
#endif

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
	m_CommandAllocator.release();
	m_BundleAllocator.release();
	m_GraphicsCommandList.release();
	m_Bundle.release();
}