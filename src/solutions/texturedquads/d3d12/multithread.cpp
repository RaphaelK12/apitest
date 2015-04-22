#include "pch.h"

#include "multithread.h"
#include "framework/gfx_dx11.h"
#include "framework/gfx_dx12.h"

extern comptr<ID3D12DescriptorHeap>			g_HeapRTV;
extern comptr<ID3D12DescriptorHeap>			g_HeapDSV;
extern comptr<ID3D12Resource>				g_BackBuffer;
extern comptr<ID3D12CommandQueue>			g_CommandQueue;
extern int	g_ClientWidth;
extern int	g_ClientHeight;

// Finish fence
extern comptr<ID3D12Fence> g_FinishFence;
extern UINT64 g_finishFenceValue;

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
TexturedQuadsD3D12MultiThread::TexturedQuadsD3D12MultiThread()
{
}

// --------------------------------------------------------------------------------------------------------------------
bool TexturedQuadsD3D12MultiThread::Init(const std::vector<TexturedQuadsProblem::Vertex>& _vertices,
	const std::vector<TexturedQuadsProblem::Index>& _indices,
	const std::vector<TextureDetails*>& _textures,
	size_t _objectCount)
{
	m_ContextId = 0;
	for (size_t i = 0; i < NUM_ACCUMULATED_FRAMES; ++i)
		m_curFenceValue[i] = 0;
	m_ThreadEnded = false;
	m_Transforms = 0;
	m_ObjectCount = _objectCount;

	if (!CreatePSO())
		return false;

	if (!CreateCommandList())
		return false;

	if (!CreateGeometryBuffer(_vertices, _indices))
		return false;

	if (!CreateTextures(_textures))
		return false;

	if (!CreateThreads())
		return false;

	return true;
}

// --------------------------------------------------------------------------------------------------------------------
void TexturedQuadsD3D12MultiThread::Render(const std::vector<Matrix>& _transforms)
{
	// Check out fence
	const UINT64 lastCompletedFence = g_FinishFence->GetCompletedValue();
	if (m_curFenceValue[m_ContextId] > lastCompletedFence)
	{
		HANDLE handleEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		g_FinishFence->SetEventOnCompletion(m_curFenceValue[m_ContextId], handleEvent);
		WaitForSingleObject(handleEvent, INFINITE);
		CloseHandle(handleEvent);
	}

	// Program
	Vec3 dir = { 0, 0, 1 };
	Vec3 at = { 0, 0, 0 };
	Vec3 up = { 0, 1, 0 };
	dir = normalize(dir);
	Vec3 eye = at - 250 * dir;
	m_ViewProjection = mProj * matrix_look_at(eye, at, up);
	m_Transforms = _transforms.data();
	
	for (int i = 0; i < NUM_EXT_THREAD; ++i)
		SetEvent(m_ThreadBeginEvent[i]);
	WaitForMultipleObjects(NUM_EXT_THREAD, m_ThreadEndEvent, TRUE, INFINITE);
	
	g_CommandQueue->ExecuteCommandLists(NUM_EXT_THREAD, (ID3D12CommandList* const*)m_CommandList[m_ContextId]);

	// setup fence
	m_curFenceValue[m_ContextId] = ++g_finishFenceValue;
	g_CommandQueue->Signal(g_FinishFence, g_finishFenceValue);

	m_ContextId = (m_ContextId + 1) % NUM_ACCUMULATED_FRAMES;
}

// --------------------------------------------------------------------------------------------------------------------
void TexturedQuadsD3D12MultiThread::Shutdown()
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

	m_RootSignature.release();
	m_PipelineState.release();
	m_GeometryBuffer.release();

	for (int i = 0; i < NUM_EXT_THREAD; ++i)
	{
		CloseHandle(m_ThreadBeginEvent[i]);
		CloseHandle(m_ThreadEndEvent[i]);
		CloseHandle(m_ThreadHandle[i]);

		for (int k = 0; k < NUM_ACCUMULATED_FRAMES; k++)
		{
			m_CommandList[k][i].release();
			m_CommandAllocator[k][i].release();
		}
	}

	m_SRVHeap.release();
	m_SamplerHeap.release();

	std::vector<comptr<ID3D12Resource>>::iterator it = m_Textures.begin();
	while (it != m_Textures.end())
	{
		it->release();
		it++;
	}
	m_Textures.clear();
}


// --------------------------------------------------------------------------------------------------------------------
bool TexturedQuadsD3D12MultiThread::CreatePSO()
{
	// compile shader first
	comptr<ID3DBlob> vsCode = CompileShader(L"textures_d3d12_naive_vs.hlsl", "vsMain", "vs_5_0");
	comptr<ID3DBlob> psCode = CompileShader(L"textures_d3d12_naive_ps.hlsl", "psMain", "ps_5_0");

	D3D12_DESCRIPTOR_RANGE descRange[2];
	descRange[0].Init(D3D12_DESCRIPTOR_RANGE_SRV, 2, 0);
	descRange[1].Init(D3D12_DESCRIPTOR_RANGE_SAMPLER, 1, 0);

	D3D12_ROOT_PARAMETER rootParameters[4];
	rootParameters[0].InitAsConstants(17, 0);
	rootParameters[1].InitAsConstants(16, 1);
	rootParameters[2].InitAsDescriptorTable(1, &descRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[3].InitAsDescriptorTable(1, &descRange[1], D3D12_SHADER_VISIBILITY_PIXEL);

	D3D12_ROOT_SIGNATURE rootSig = { 4, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
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
		{ "TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_PER_VERTEX_DATA, 0 },
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

// --------------------------------------------------------------------------------------------------------------------
bool TexturedQuadsD3D12MultiThread::CreateCommandList()
{
	for (int i = 0; i < NUM_EXT_THREAD; ++i)
	{
		for (int k = 0; k < NUM_ACCUMULATED_FRAMES; k++)
		{
			// create command queue allocator
			HRESULT hr = g_D3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&m_CommandAllocator[k][i]);
			if (FAILED(hr))
				return false;

			// Create Command List
			g_D3D12Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator[k][i], 0, __uuidof(ID3D12GraphicsCommandList), (void**)&m_CommandList[k][i]);
			m_CommandList[k][i]->Close();
		}
	}

	return true;
}

bool TexturedQuadsD3D12MultiThread::CreateGeometryBuffer(const std::vector<TexturedQuadsProblem::Vertex>& _vertices,
	const std::vector<TexturedQuadsProblem::Index>& _indices)
{
	const size_t sizeofVertex = sizeof(TexturedQuadsProblem::Vertex);
	const size_t sizeofVertices = sizeofVertex * _vertices.size();
	const size_t sizeofIndex = sizeof(TexturedQuadsProblem::Index);
	const size_t sizeofIndices = sizeofIndex * _indices.size();
	const size_t totalSize = sizeofIndices + sizeofIndices;

	if (FAILED(g_D3D12Device->CreateCommittedResource(
		&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_MISC_NONE,
		&CD3D12_RESOURCE_DESC::Buffer(totalSize),
		D3D12_RESOURCE_USAGE_GENERIC_READ,
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

bool TexturedQuadsD3D12MultiThread::CreateTextures(const std::vector<TextureDetails*>& _textures)
{
	m_SRVDescriptorSize = g_D3D12Device->GetDescriptorHandleIncrementSize(D3D12_SAMPLER_DESCRIPTOR_HEAP);
	m_SamplerDescriptorSize = g_D3D12Device->GetDescriptorHandleIncrementSize(D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP);

	// Create sampler descriptor heap
	// One clamp and one wrap sampler
	D3D12_DESCRIPTOR_HEAP_DESC descHeapSampler = {};
	descHeapSampler.NumDescriptors = 1;
	descHeapSampler.Type = D3D12_SAMPLER_DESCRIPTOR_HEAP;
	descHeapSampler.Flags = D3D12_DESCRIPTOR_HEAP_SHADER_VISIBLE;
	HRESULT hr = g_D3D12Device->CreateDescriptorHeap(&descHeapSampler, __uuidof(ID3D12DescriptorHeap), (void **)&m_SamplerHeap);
	if (FAILED(hr))
		return false;

	D3D12_SAMPLER_DESC sampleDesc;
	ZeroMemory(&sampleDesc, sizeof(sampleDesc));
	sampleDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampleDesc.AddressU = D3D12_TEXTURE_ADDRESS_WRAP;
	sampleDesc.AddressV = D3D12_TEXTURE_ADDRESS_WRAP;
	sampleDesc.AddressW = D3D12_TEXTURE_ADDRESS_WRAP;
	sampleDesc.MinLOD = 0;
	sampleDesc.MaxLOD = D3D12_FLOAT32_MAX;
	sampleDesc.MipLODBias = 0.0f;
	sampleDesc.MaxAnisotropy = 1;
	sampleDesc.ComparisonFunc = D3D12_COMPARISON_NEVER;

	// Create sampler
	g_D3D12Device->CreateSampler(&sampleDesc, m_SamplerHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_DESCRIPTOR_HEAP_DESC descHeapCbvSrv = {};
	descHeapCbvSrv.NumDescriptors = _textures.size();
	descHeapCbvSrv.Type = D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP;
	descHeapCbvSrv.Flags = D3D12_DESCRIPTOR_HEAP_SHADER_VISIBLE;
	hr = g_D3D12Device->CreateDescriptorHeap(&descHeapCbvSrv, __uuidof(ID3D12DescriptorHeap), (void **)&m_SRVHeap);
	if (FAILED(hr))
		return false;

	// get a handle to the start of the descriptor heap
	D3D12_CPU_DESCRIPTOR_HANDLE cbvSRVHandle = m_SRVHeap->GetCPUDescriptorHandleForHeapStart();

	std::vector<TextureDetails*>::const_iterator it = _textures.begin();
	while (it != _textures.end())
	{
		ID3D12Resource* texture = NewTextureFromDetails(*(*it));

		// create shader resource view descriptor
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {};
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Format = (DXGI_FORMAT)(*it)->d3dFormat;
		shaderResourceViewDesc.Texture2D.MipLevels = (*it)->szMipMapCount;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
		shaderResourceViewDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		g_D3D12Device->CreateShaderResourceView(texture, &shaderResourceViewDesc, cbvSRVHandle);

		// move to the next descriptor slot
		cbvSRVHandle.Offset(m_SRVDescriptorSize);

		// push the textures
		m_Textures.push_back(texture);

		it++;
	}

	return true;
}


bool TexturedQuadsD3D12MultiThread::CreateThreads()
{
	struct PackedData
	{
		int		thread_id;
		TexturedQuadsD3D12MultiThread*	instance;
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

void TexturedQuadsD3D12MultiThread::renderMultiThread(int tid)
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

void TexturedQuadsD3D12MultiThread::RenderPart(int pid, int total)
{
	if (FAILED(m_CommandAllocator[m_ContextId][pid]->Reset()))
		return;

	// Reset command list
	m_CommandList[m_ContextId][pid]->Reset(m_CommandAllocator[m_ContextId][pid], m_PipelineState);

	// Setup root signature
	m_CommandList[m_ContextId][pid]->SetGraphicsRootSignature(m_RootSignature);

	// Setup viewport
	D3D12_VIEWPORT viewport = { 0, 0, FLOAT(g_ClientWidth), FLOAT(g_ClientHeight), 0.0f, 1.0f };
	m_CommandList[m_ContextId][pid]->RSSetViewports(1, &viewport);

	// Setup scissor
	D3D12_RECT scissorRect = { 0, 0, g_ClientWidth, g_ClientHeight };
	m_CommandList[m_ContextId][pid]->RSSetScissorRects(1, &scissorRect);

	// Set Render Target
	m_CommandList[m_ContextId][pid]->SetRenderTargets(&g_HeapRTV->GetCPUDescriptorHandleForHeapStart(), true, 1, &g_HeapDSV->GetCPUDescriptorHandleForHeapStart());

	// Draw the triangle
	m_CommandList[m_ContextId][pid]->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_CommandList[m_ContextId][pid]->SetVertexBuffers(0, &m_VertexBufferView, 1);
	m_CommandList[m_ContextId][pid]->SetIndexBuffer(&m_IndexBufferView);

	ID3D12DescriptorHeap*	heaps[2] = { m_SRVHeap, m_SamplerHeap };
	m_CommandList[m_ContextId][pid]->SetDescriptorHeaps(heaps, 2);

	m_CommandList[m_ContextId][pid]->SetGraphicsRoot32BitConstants(1, &m_ViewProjection, 0, 16);
	m_CommandList[m_ContextId][pid]->SetGraphicsRootDescriptorTable(2, m_SRVHeap->GetGPUDescriptorHandleForHeapStart());
	m_CommandList[m_ContextId][pid]->SetGraphicsRootDescriptorTable(3, m_SamplerHeap->GetGPUDescriptorHandleForHeapStart());

	size_t start = m_ObjectCount / total * pid;
	size_t end = start + m_ObjectCount / total;
	if (end > m_ObjectCount)
		end = m_ObjectCount;
	ConstantsPerDraw perDrawData;
	for (unsigned int u = start; u < end; ++u) {
		perDrawData.World = m_Transforms[u];
		perDrawData.InstanceId = u;
		m_CommandList[m_ContextId][pid]->SetGraphicsRoot32BitConstants(0, &perDrawData, 0, 17);
		m_CommandList[m_ContextId][pid]->DrawIndexedInstanced(m_IndexCount, 1, 0, 0, u);
	}
	m_CommandList[m_ContextId][pid]->Close();
}