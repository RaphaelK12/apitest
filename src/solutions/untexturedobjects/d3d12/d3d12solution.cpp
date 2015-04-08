#include "pch.h"
#include "d3d12solution.h"
#include "framework/gfx_dx12.h"
#include "framework/gfx_dx11.h"	// for some utility function

extern ID3D12DescriptorHeap*	g_HeapRTV;
extern ID3D12Resource*			g_BackBuffer;
extern int	g_ClientWidth;
extern int	g_ClientHeight;

UntexturedD3D12Solution::UntexturedD3D12Solution():
m_VertexBuffer(),
m_IndexBuffer(),
m_PipelineState(),
m_ConstantBuffer(),
m_DescriptorHeap(),
m_RootSignature(),
m_BufferHeap()
{
}

bool UntexturedD3D12Solution::Init(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
	const std::vector<UntexturedObjectsProblem::Index>& _indices,
	size_t _objectCount)
{
	const int size = 8 * 1024 * 1024;
	if (sizeof(UntexturedObjectsProblem::Vertex) * _vertices.size() + sizeof(UntexturedObjectsProblem::Index) * _indices.size() > size)
		return false;	// not enough memory allocated

	if (FAILED(g_D3D12Device->CreateHeap(
		&CD3D12_HEAP_DESC(size, D3D12_HEAP_TYPE_UPLOAD, 0, D3D12_HEAP_MISC_DENY_TEXTURES),
		__uuidof(ID3D12Heap),
		reinterpret_cast<void**>(&m_BufferHeap))))
		return false;

	m_VertexBuffer = CreateBufferFromVector(_vertices, m_BufferHeap , 0 );
	m_IndexBuffer = CreateBufferFromVector(_indices, m_BufferHeap, 0x10000 );
	if (!m_VertexBuffer || !m_IndexBuffer)
		return false;

	m_IndexCount = _indices.size();

	m_VertexBufferView = D3D12_VERTEX_BUFFER_VIEW{ m_VertexBuffer->GetGPUVirtualAddress(), sizeof(UntexturedObjectsProblem::Vertex) * _vertices.size(), sizeof(UntexturedObjectsProblem::Vertex) };
	m_IndexBufferView = D3D12_INDEX_BUFFER_VIEW{ m_IndexBuffer->GetGPUVirtualAddress(), sizeof(UntexturedObjectsProblem::Index), DXGI_FORMAT_R16_UINT };

	if (!CreatePSO())
		return false;

	if (!CreateConstantBuffer())
		return false;

	if (!CreateCommandAllocator())
		return false;

	return true;
}

bool UntexturedD3D12Solution::CreatePSO()
{
	// compile shader first
	comptr<ID3DBlob> vsCode = CompileShader(L"cubes_d3d12_naive_vs.hlsl", "vsMain", "vs_5_0");
	comptr<ID3DBlob> psCode = CompileShader(L"cubes_d3d12_naive_ps.hlsl", "psMain", "ps_5_0");

	// Create Root signature
	D3D12_DESCRIPTOR_RANGE descRanges[1];
	descRanges[0].Init(D3D12_DESCRIPTOR_RANGE_CBV, 1, 0);

	D3D12_ROOT_PARAMETER rootParameters[1];
	rootParameters[0].InitAsDescriptorTable(1, &descRanges[0], D3D12_SHADER_VISIBILITY_ALL);

	D3D12_ROOT_SIGNATURE rootSig = { 1, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
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

	return SUCCEEDED(g_D3D12Device->CreateGraphicsPipelineState(&psod, __uuidof(ID3D12PipelineState), (void**)&m_PipelineState));
}

bool UntexturedD3D12Solution::CreateConstantBuffer()
{
	const D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP, 1000, D3D12_DESCRIPTOR_HEAP_SHADER_VISIBLE };
	if (FAILED(g_D3D12Device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&m_DescriptorHeap))))
	{
		return false;
	}

	// Create a (large) constant buffer
	if (FAILED(g_D3D12Device->CreateCommittedResource(
		&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_MISC_NONE,
		&CD3D12_RESOURCE_DESC::Buffer( sizeof( ConstantsBufferData ) ),
		D3D12_RESOURCE_USAGE_GENERIC_READ,
		nullptr,
		__uuidof(ID3D12Resource),
		reinterpret_cast<void**>(&m_ConstantBuffer))))
	{
		return false;
	}

	// Update constants
	UINT8* pRaw = nullptr;
	m_ConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_ConstantBufferData));

	// Program
	Vec3 dir = { -0.5f, -1, 1 };
	Vec3 at = { 0, 0, 0 };
	Vec3 up = { 0, 0, 1 };
	dir = normalize(dir);
	Vec3 eye = at - 250 * dir;
	Matrix view = matrix_look_at(eye, at, up);
	//m_ConstantBufferData->ViewProjection.FromMatrix(mProj * view);
	//m_ConstantBufferData->World.FromMatrix(transpose(_transforms[0]));

	m_ConstantBufferData->ViewProjection.IdentityMatrix();
	m_ConstantBufferData->World.IdentityMatrix();

	//m_ConstantBuffer->Unmap(0, 0);

	// Create a constant buffer descriptor
	D3D12_CONSTANT_BUFFER_VIEW_DESC cdesc = { m_ConstantBuffer->GetGPUVirtualAddress(), sizeof(ConstantsBufferData) };
	g_D3D12Device->CreateConstantBufferView(&cdesc, m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	
	return true;
}

bool UntexturedD3D12Solution::CreateCommandAllocator()
{
	// Create a command allocator
	if (FAILED(g_D3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&m_CommandAllocator))))
		return false;

	if (FAILED(g_D3D12Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator, m_PipelineState, __uuidof(ID3D12CommandList), (void**)&m_GraphicsCommandList)))
		return false;
	m_GraphicsCommandList->Close();

	return true;
}

void UntexturedD3D12Solution::Render(const std::vector<Matrix>& _transforms)
{
	if (FAILED(m_CommandAllocator->Reset()))
		return;

	// Update constants
	UINT8* pRaw = nullptr;
	m_ConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_ConstantBufferData));

	// Program
	Vec3 dir = { -0.5f, -1, 1 };
	Vec3 at = { 0, 0, 0 };
	Vec3 up = { 0, 0, 1 };
	dir = normalize(dir);
	Vec3 eye = at - 250 * dir;
	Matrix view = matrix_look_at(eye, at, up);
	//m_ConstantBufferData->ViewProjection.FromMatrix(mProj * view);
	//m_ConstantBufferData->World.FromMatrix(transpose(_transforms[0]));

	m_ConstantBufferData->ViewProjection.IdentityMatrix();
	m_ConstantBufferData->World.IdentityMatrix();

	m_ConstantBuffer->Unmap(0, 0);

	// Create Command List first time invoked
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
		m_GraphicsCommandList->SetRenderTargets(&g_HeapRTV->GetCPUDescriptorHandleForHeapStart(), true, 1, nullptr);

		//m_GraphicsCommandList->SetPipelineState(m_PipelineState);
		m_GraphicsCommandList->SetGraphicsRootSignature(m_RootSignature);

		// Setup constant buffer
		m_GraphicsCommandList->SetGraphicsRootDescriptorTable(0, m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		
		// Draw the triangle
		m_GraphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_GraphicsCommandList->SetVertexBuffers(0, &m_VertexBufferView, 1);
		m_GraphicsCommandList->SetIndexBuffer(&m_IndexBufferView);

		size_t xformCount = _transforms.size();
		for (size_t u = 0; u < 1; ++u) {
			// draw the instance
			m_GraphicsCommandList->DrawInstanced(m_IndexCount, 1, 0, 0);
		}

		// Close the command list
		m_GraphicsCommandList->Close();
	}

	// Execute Command List
	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_GraphicsCommandList);
}

void UntexturedD3D12Solution::Shutdown()
{
	SafeRelease(m_GraphicsCommandList);
	SafeRelease(m_CommandAllocator);
	SafeRelease(m_VertexBuffer);
	SafeRelease(m_IndexBuffer);
	SafeRelease(m_PipelineState);
	SafeRelease(m_ConstantBuffer);
	SafeRelease(m_DescriptorHeap);
	SafeRelease(m_RootSignature);
}