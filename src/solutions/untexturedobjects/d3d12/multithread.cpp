#include "pch.h"
#include "multithread.h"
#include "framework/gfx_dx12.h"
#include "framework/gfx_dx11.h"	// for some utility function
#include <d3dcompiler.h>

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

UntexturedObjectsD3D12MultiThread::UntexturedObjectsD3D12MultiThread()
{
}

bool UntexturedObjectsD3D12MultiThread::Init(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
	const std::vector<UntexturedObjectsProblem::Index>& _indices,
	size_t _objectCount)
{
	m_ObjectCount = _objectCount;

	if (!CreatePSO())
		return false;

	if (!CreateGeometryBuffer(_vertices, _indices))
		return false;

	if (!CreateThreads())
		return false;

	if (!CreateCommands())
		return false;

	return true;
}

bool UntexturedObjectsD3D12MultiThread::CreatePSO()
{
	// compile shader first
	comptr<ID3DBlob> vsCode = CompileShader(L"cubes_d3d12_naive_vs.hlsl", "vsMain", "vs_5_0");
	comptr<ID3DBlob> psCode = CompileShader(L"cubes_d3d12_naive_ps.hlsl", "psMain", "ps_5_0");

	D3D12_ROOT_PARAMETER rootParameters[2];
	memset(rootParameters, 0, sizeof(rootParameters));
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[0].Constants.Num32BitValues = 16;
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

	if (FAILED(g_D3D12Device->CreateGraphicsPipelineState(&psod, __uuidof(ID3D12PipelineState), (void**)&m_PipelineState)))
		return false;
	
	return true;
}

bool UntexturedObjectsD3D12MultiThread::CreateGeometryBuffer(const std::vector<UntexturedObjectsProblem::Vertex>& _vertices,
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

bool UntexturedObjectsD3D12MultiThread::CreateCommands()
{
	// Create Command List
	for (int i = 0; i < NUM_EXT_THREAD; ++i)
	{
		for (int k = 0; k < NUM_ACCUMULATED_FRAMES; ++k)
		{
			if (FAILED(g_D3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&m_CommandAllocator[k][i])))
				return false;

			g_D3D12Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator[k][i], 0, __uuidof(ID3D12GraphicsCommandList), (void**)&m_CommandList[k][i]);
			m_CommandList[k][i]->Close();
		}
	}

	return true;
}

void UntexturedObjectsD3D12MultiThread::Render(const std::vector<Matrix>& _transforms)
{
	// Program
	Vec3 dir = { -0.5f, -1, 1 };
	Vec3 at = { 0, 0, 0 };
	Vec3 up = { 0, 0, 1 };
	dir = normalize(dir);
	Vec3 eye = at - 250.0f * dir;
	
	m_ViewProjection = mProj * matrix_look_at(eye, at, up);
	m_Transforms = _transforms.data();

	for (int i = 0; i < NUM_EXT_THREAD; ++i)
		SetEvent(m_ThreadBeginEvent[i]);
	WaitForMultipleObjects(NUM_EXT_THREAD, m_ThreadEndEvent, TRUE, INFINITE);

	// execute command list
	g_CommandQueue->ExecuteCommandLists(NUM_EXT_THREAD, (ID3D12CommandList* const*)m_CommandList[g_curContext]);
}

void UntexturedObjectsD3D12MultiThread::Shutdown()
{
	// Make sure everything is flushed out before releasing anything.
	HANDLE handleEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
	g_CommandQueue->Signal(g_FinishFence, ++g_finishFenceValue);
	g_FinishFence->SetEventOnCompletion(g_finishFenceValue, handleEvent);
	WaitForSingleObject(handleEvent, INFINITE);
	CloseHandle(handleEvent);

	m_ThreadEnded = true;
	for (int i = 0; i < NUM_EXT_THREAD; ++i)
		SetEvent(m_ThreadBeginEvent[i]);
	WaitForMultipleObjects(NUM_EXT_THREAD, m_ThreadHandle, 1, INFINITE);

	m_GeometryBuffer.release();
	m_RootSignature.release();
	m_PipelineState.release();

	// Close thread events and thread handles
	for (int i = 0; i < NUM_EXT_THREAD; i++)
	{
		CloseHandle(m_ThreadBeginEvent[i]);
		CloseHandle(m_ThreadEndEvent[i]);
		CloseHandle(m_ThreadHandle[i]);

		for (int k = 0; k < NUM_ACCUMULATED_FRAMES; k++)
		{
			m_CommandAllocator[k][i].release();
			m_CommandList[k][i].release();
		}
	}
}

bool UntexturedObjectsD3D12MultiThread::CreateThreads()
{
	struct PackedData
	{
		int		thread_id;
		UntexturedObjectsD3D12MultiThread*	instance;
	};
	struct threadwrapper
	{
		static unsigned int WINAPI thunk(LPVOID lpParameter)
		{
			PackedData* data = (PackedData*)(lpParameter);
			while (true)
			{
				data->instance->renderMultiThread(data->thread_id);
			}
			return 0;
		}
	};

	m_ThreadEnded = false;

	static PackedData data[NUM_EXT_THREAD];
	for (int i = 0; i < NUM_EXT_THREAD; i++)
	{
		m_ThreadBeginEvent[i] = CreateEvent(
			NULL,
			FALSE,
			FALSE,
			NULL);

		m_ThreadEndEvent[i] = CreateEvent(
			NULL,
			FALSE,
			FALSE,
			NULL);

		data[i].instance = this;
		data[i].thread_id = i;

		m_ThreadHandle[i] = (HANDLE)_beginthreadex(
			nullptr,
			0,
			threadwrapper::thunk,
			(LPVOID)&data[i], // Thread ID
			0,
			nullptr);

		// failed to create threads
		if (!m_ThreadBeginEvent || !m_ThreadEndEvent[i] || !m_ThreadHandle[i])
			return false;
	}

	return true;
}

void UntexturedObjectsD3D12MultiThread::renderMultiThread(int tid)
{
	// wait for rendering
	WaitForSingleObject(m_ThreadBeginEvent[tid], INFINITE);

	// end thread if necessary
	if (m_ThreadEnded)
	{
		_endthread();
		return;
	}

	// something temporary
	RenderPart(tid, NUM_EXT_THREAD);

	// finish filling command buffer
	SetEvent(m_ThreadEndEvent[tid]);
}

void UntexturedObjectsD3D12MultiThread::RenderPart(int pid, int total)
{
	// reset command list
	m_CommandAllocator[g_curContext][pid]->Reset();
	m_CommandList[g_curContext][pid]->Reset(m_CommandAllocator[g_curContext][pid], m_PipelineState);
	
	// Setup root signature
	m_CommandList[g_curContext][pid]->SetGraphicsRootSignature(m_RootSignature);

	// Setup viewport
	D3D12_VIEWPORT viewport = { 0, 0, FLOAT(g_ClientWidth), FLOAT(g_ClientHeight), 0.0f, 1.0f };
	m_CommandList[g_curContext][pid]->RSSetViewports(1, &viewport);

	// Setup scissor
	D3D12_RECT scissorRect = { 0, 0, g_ClientWidth, g_ClientHeight };
	m_CommandList[g_curContext][pid]->RSSetScissorRects(1, &scissorRect);

	// Set Render Target
	m_CommandList[g_curContext][pid]->OMSetRenderTargets(1, &g_HeapRTV->GetCPUDescriptorHandleForHeapStart(), true, &g_HeapDSV->GetCPUDescriptorHandleForHeapStart());
	
	// Draw the triangle
	m_CommandList[g_curContext][pid]->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_CommandList[g_curContext][pid]->IASetVertexBuffers(0, 1, &m_VertexBufferView);
	m_CommandList[g_curContext][pid]->IASetIndexBuffer(&m_IndexBufferView);

	// setup view projection matrix
	m_CommandList[g_curContext][pid]->SetGraphicsRoot32BitConstants(1, 16, &m_ViewProjection, 0);
	
	size_t start = m_ObjectCount / total * pid;
	size_t end = start + m_ObjectCount / total;
	unsigned int counter = 0;
	for (unsigned int u = start; u < end; ++u) {
		m_CommandList[g_curContext][pid]->SetGraphicsRoot32BitConstants(0, 16, &(m_Transforms[u]), 0);
		m_CommandList[g_curContext][pid]->DrawIndexedInstanced(m_IndexCount, 1, 0, 0, 0);
	}
	
	m_CommandList[g_curContext][pid]->Close();
}