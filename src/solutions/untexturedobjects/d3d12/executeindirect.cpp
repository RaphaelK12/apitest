#include "pch.h"
#include "executeindirect.h"
#include "framework/gfx_dx12.h"
#include "framework/gfx_dx11.h"	// for some utility function
#include <d3dcompiler.h>

#define CBV_INEXECUTEINDIRECT 0

extern comptr<ID3D12CommandQueue>			g_CommandQueue;
extern int	g_ClientWidth;
extern int	g_ClientHeight;
extern int	g_curContext;

// Finish fence
extern comptr<ID3D12Fence> g_FinishFence;
extern UINT64 g_finishFenceValue;

static int _count = 0;

UntexturedObjectsD3D12ExecuteIndirect::UntexturedObjectsD3D12ExecuteIndirect()
{
}

bool UntexturedObjectsD3D12ExecuteIndirect::Init(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
	const std::vector<UntexturedObjectsProblem::Index>& _indices,
	size_t _objectCount)
{
	if (!CreatePSO())
		return false;

	if (!CreateGeometryBuffer(_vertices, _indices))
		return false;

	if (!CreateConstantBuffer(_objectCount))
		return false;

	if (!CreateCommandSignature(_objectCount))
		return false;

	if (!CreateCommandList())
		return false;

	_count = 0;

	return true;
}

bool UntexturedObjectsD3D12ExecuteIndirect::CreatePSO()
{
	// compile shader first
	comptr<ID3DBlob> vsCode = CompileShader(L"cubes_d3d12_naive_vs.hlsl", "vsMain", "vs_5_0");
	comptr<ID3DBlob> psCode = CompileShader(L"cubes_d3d12_naive_ps.hlsl", "psMain", "ps_5_0");

	// Create Root signature
	D3D12_ROOT_PARAMETER rootParameters[2];
	memset(rootParameters, 0, sizeof(rootParameters));
#if CBV_INEXECUTEINDIRECT
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
#else
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[0].Constants.Num32BitValues = 16;
	rootParameters[0].Constants.ShaderRegister = 0;
#endif

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

bool UntexturedObjectsD3D12ExecuteIndirect::CreateGeometryBuffer(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
	const std::vector<UntexturedObjectsProblem::Index>& _indices)
{
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

bool UntexturedObjectsD3D12ExecuteIndirect::CreateCommandSignature(size_t count)
{
	for (int u = 0; u < NUM_ACCUMULATED_FRAMES; u++)
	{
		struct CustomData
		{
#if CBV_INEXECUTEINDIRECT
			D3D12_GPU_VIRTUAL_ADDRESS	 cbv;
#else
			Matrix	 matrix;
#endif
			D3D12_DRAW_INDEXED_ARGUMENTS arg;
			char padding[16];

			CustomData()
			{
				memset(this, 0, sizeof(CustomData));
			}
		};
		const int sizeofCD = sizeof(CustomData);

		int byteStride = sizeofCD;

		// It doesn't work here somehow, maybe come back later
		// Updating Constant through set32bitsconstant parameter works here, however it requires updating the buffer every frame.
		D3D12_INDIRECT_ARGUMENT_DESC desc[2];
		memset(desc, 0, sizeof(desc));
#if CBV_INEXECUTEINDIRECT
		desc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
		desc[0].ConstantBufferView.RootParameterIndex = 0;
#else
		desc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
		desc[0].Constant.Num32BitValuesToSet = 16;
#endif
		desc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
		desc[1].VertexBuffer.Slot = 0;

		D3D12_COMMAND_SIGNATURE_DESC para;
		memset(&para, 0, sizeof(para));
		para.NumArgumentDescs = 2;
		para.pArgumentDescs = &desc[0];
		para.ByteStride = byteStride;

		if (FAILED(g_D3D12Device->CreateCommandSignature(&para, m_RootSignature, __uuidof(ID3D12CommandSignature), reinterpret_cast<void**>(&m_CommandSig[u]))))
			return false;

		if (FAILED(g_D3D12Device->CreateCommittedResource(
			&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3D12_RESOURCE_DESC::Buffer(count*sizeofCD),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_CommandBuffer[u])
			)))
			return false;

#if CBV_INEXECUTEINDIRECT
		std::vector<CustomData> args;
		args.resize(count);
		for (size_t i = 0; i < count; ++i)
		{
			args[i].cbv = m_ConstantBuffer[u]->GetGPUVirtualAddress() + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT * i;
			args[i].arg.BaseVertexLocation = 0;
			args[i].arg.IndexCountPerInstance = 36;
			args[i].arg.InstanceCount = 1;
			args[i].arg.StartIndexLocation = 0;
			args[i].arg.StartInstanceLocation = 0;
		}

		UINT8* pRaw = 0;
		m_CommandBuffer[u]->Map(0, 0, reinterpret_cast<void**>(&pRaw));
		memcpy(pRaw, args.data(), count*sizeofCD);
		m_CommandBuffer[u]->Unmap(0, 0);
#endif
	}

	return true;
}

bool UntexturedObjectsD3D12ExecuteIndirect::CreateConstantBuffer(size_t count)
{
	for (int u = 0; u < NUM_ACCUMULATED_FRAMES; u++)
	{
		// Create a (large) constant buffer
		if (FAILED(g_D3D12Device->CreateCommittedResource(
			&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3D12_RESOURCE_DESC::Buffer(count * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			__uuidof(ID3D12Resource),
			reinterpret_cast<void**>(&m_ConstantBuffer[u]))))
		{
			return false;
		}

		m_ConstantBuffer[u]->Map(0, nullptr, reinterpret_cast<void**>(&m_ConstantBufferData[u]));
	}

	return true;
}

void UntexturedObjectsD3D12ExecuteIndirect::Render(const std::vector<Matrix>& _transforms)
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

#if CBV_INEXECUTEINDIRECT
	for (unsigned int i = 0; i < (unsigned int)count; ++i)
		m_ConstantBufferData[g_curContext][4 * i].m = _transforms[i];
#else
	
	if (_count<NUM_ACCUMULATED_FRAMES)
	{
		// ----------------------------------------------------------------------------------
		// The following code could be removed if CBV in execute indirect works as expected
		UINT8* pRaw = 0;
		struct CustomData
		{
			Matrix	 matrix;
			D3D12_DRAW_INDEXED_ARGUMENTS arg;
			char padding[16];

			CustomData()
			{
				memset(this, 0, sizeof(CustomData));
			}
		};
		const int sizeofCD = sizeof(CustomData);
		std::vector<CustomData> args;
		args.resize(count);
		for (size_t i = 0; i < count; ++i)
		{
			args[i].matrix = _transforms[i];
			args[i].arg.BaseVertexLocation = 0;
			args[i].arg.IndexCountPerInstance = 36;
			args[i].arg.InstanceCount = 1;
			args[i].arg.StartIndexLocation = 0;
			args[i].arg.StartInstanceLocation = 0;
		}
		m_CommandBuffer[g_curContext]->Map(0, 0, reinterpret_cast<void**>(&pRaw));
		memcpy(pRaw, args.data(), count*sizeofCD);
		++_count;
	}
#endif
	// --------------------------------------------------------------------------------------

	Matrix vp = mProj * matrix_look_at(eye, at, up);

	// Create Command List first time invoked
	{
		// Reset command list
		m_CommandList[g_curContext]->Reset(m_CommandAllocator[g_curContext], 0);

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

		// Setup pipeline state
		m_CommandList[g_curContext]->SetPipelineState(m_PipelineState);

		// Draw the triangle
		m_CommandList[g_curContext]->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_CommandList[g_curContext]->IASetVertexBuffers(0, 1, &m_VertexBufferView);
		m_CommandList[g_curContext]->IASetIndexBuffer(&m_IndexBufferView);

		m_CommandList[g_curContext]->SetGraphicsRoot32BitConstants(1, 16, &vp, 0);

		// execute indirect
		m_CommandList[g_curContext]->ExecuteIndirect(m_CommandSig[g_curContext], count, m_CommandBuffer[g_curContext], 0, 0, 0);
		
		/*for (unsigned int u = 0; u < count / 10; ++u) {
		//	m_CommandList[g_curContext]->SetGraphicsRootConstantBufferView(0, m_ConstantBuffer->GetGPUVirtualAddress() + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT * u);
			//m_CommandList->DrawIndexedInstanced(m_IndexCount, 1, 0, 0, 0);

			m_CommandList[g_curContext]->ExecuteIndirect(m_CommandSig[g_curContext], 1, m_CommandBuffer[g_curContext], 0, 0, 0);
		}*/
		
		m_CommandList[g_curContext]->Close();
	}

	g_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_CommandList[g_curContext]);
}

void UntexturedObjectsD3D12ExecuteIndirect::Shutdown()
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
	m_GeometryBufferHeap.release();

	for (int k = 0; k < NUM_ACCUMULATED_FRAMES; k++)
	{
		m_CommandAllocator[k].release();
		m_CommandList[k].release();
		m_ConstantBuffer[k].release();
		m_CommandBuffer[k].release();
		m_CommandSig[k].release();
	}
}

bool UntexturedObjectsD3D12ExecuteIndirect::CreateCommandList()
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