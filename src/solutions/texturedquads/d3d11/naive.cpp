#include "pch.h"

#include "naive.h"
#include "framework/gfx_dx11.h"

template class TexturedQuadsD3D11Naive<GfxApiDirect3D11>;
template class TexturedQuadsD3D11Naive<GfxApiDirect3D11On12>;

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
template<class T>
TexturedQuadsD3D11Naive<T>::TexturedQuadsD3D11Naive()
: mInputLayout()
, mConstantBufferPerFrame()
, mVertexShader()
, mPixelShader()
, mSamplerState()
, mRasterizerState()
, mBlendState()
, mDepthStencilState()
, mVertexBuffer()
{}

// --------------------------------------------------------------------------------------------------------------------
template<class T>
bool TexturedQuadsD3D11Naive<T>::Init(const std::vector<TexturedQuadsProblem::Vertex>& _vertices,
                                   const std::vector<TexturedQuadsProblem::Index>& _indices,
                                   const std::vector<TextureDetails*>& _textures,
                                   size_t _objectCount)
{
	if (T::m_d3d_device == 0)
		return false;

    if (!TexturedQuadsSolution::Init(_vertices, _indices, _textures, _objectCount)) {
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC elements[] =
    {
        { "ObjPos",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if (!CompileProgram(L"textures_d3d11_naive_vs.hlsl", &mVertexShader, 
                        L"textures_d3d11_naive_ps.hlsl", &mPixelShader,
						ArraySize(elements), elements, &mInputLayout, T::m_d3d_device)) {
        return false;
    }

    // Constant Buffer
	HRESULT hr = CreateConstantBuffer(sizeof(ConstantsPerFrame), nullptr, &mConstantBufferPerFrame, T::m_d3d_device);
    if (FAILED(hr)) {
        return false;
    }

	hr = CreateConstantBuffer(sizeof(ConstantsPerDraw), nullptr, &mConstantBufferPerDraw, T::m_d3d_device);
    if (FAILED(hr)) {
        return false;
    }

    // Textures
    for (auto it = _textures.begin(); it != _textures.end(); ++it) {
		ID3D11ShaderResourceView* texSRV = NewTexture2DSRVFromDetails(*(*it), T::m_d3d_device);
        if (!texSRV) {
            console::warn("Unable to initialize solution '%s', texture creation failed.", GetName().c_str());
            return false;
        }

        // Needs to be freed later.
        mTextureSRVs.push_back(texSRV);
    }

    // Render States
    {
        D3D11_RASTERIZER_DESC desc;
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.FrontCounterClockwise = FALSE;
        desc.DepthBias = 0;
        desc.SlopeScaledDepthBias = 0.0f;
        desc.DepthBiasClamp = 0.0f;
        desc.DepthClipEnable = FALSE;
        desc.ScissorEnable = FALSE;
        desc.MultisampleEnable = FALSE;
        desc.AntialiasedLineEnable = FALSE;

        hr = T::m_d3d_device->CreateRasterizerState(&desc, &mRasterizerState);
        if (FAILED(hr)) {
            return false;
        }
    }

    {
        D3D11_BLEND_DESC desc;
        desc.AlphaToCoverageEnable = FALSE;
        desc.IndependentBlendEnable = FALSE;

        D3D11_RENDER_TARGET_BLEND_DESC& rtDesc = desc.RenderTarget[0];
        rtDesc.BlendEnable = TRUE;
        rtDesc.SrcBlend = D3D11_BLEND_SRC_ALPHA;
        rtDesc.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        rtDesc.BlendOp = D3D11_BLEND_OP_ADD;
        rtDesc.SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
        rtDesc.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        rtDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        rtDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        hr = T::m_d3d_device->CreateBlendState(&desc, &mBlendState);
        if (FAILED(hr)) {
            return false;
        }
    }

    {
        D3D11_DEPTH_STENCIL_DESC desc;
        desc.DepthEnable = FALSE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_LESS;
        desc.StencilEnable = FALSE;
        desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
        desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
        desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

        hr = T::m_d3d_device->CreateDepthStencilState(&desc, &mDepthStencilState);
        if (FAILED(hr)) {
            return false;
        }
    }

    {
        D3D11_SAMPLER_DESC desc;
        desc.Filter = D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.MipLODBias = 0.0f;
        desc.MaxAnisotropy = 0;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.BorderColor[0] = 0.0f;
        desc.BorderColor[1] = 0.0f;
        desc.BorderColor[2] = 0.0f;
        desc.BorderColor[3] = 0.0f;
        desc.MinLOD = 0;
        desc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = T::m_d3d_device->CreateSamplerState(&desc, &mSamplerState);
        if (FAILED(hr)) {
            return false;
        }
    }

	mVertexBuffer = CreateBufferFromVector(T::m_d3d_device, _vertices, D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER);
    if (!mVertexBuffer) {
        return false;
    }

	mIndexBuffer = CreateBufferFromVector(T::m_d3d_device, _indices, D3D11_USAGE_IMMUTABLE, D3D11_BIND_INDEX_BUFFER);
    if (!mVertexBuffer) {
        return false;
    }

    mIndexCount = _indices.size();

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
template<class T>
void TexturedQuadsD3D11Naive<T>::Render(const std::vector<Matrix>& _transforms)
{
    // Program
    Vec3 dir = { 0, 0, 1 };
    Vec3 at = { 0, 0, 0 };
    Vec3 up = { 0, 1, 0 };
    dir = normalize(dir);
    Vec3 eye = at - 250 * dir;
    Matrix view = matrix_look_at(eye, at, up);
    ConstantsPerFrame cFrame;
    cFrame.ViewProjection = mProj * view;
    

    T::m_d3d_context->UpdateSubresource(mConstantBufferPerFrame, 0, nullptr, &cFrame, 0, 0);

    ID3D11Buffer* ia_buffers[] = { mVertexBuffer };
    UINT ia_strides[] = { sizeof(TexturedQuadsProblem::Vertex) };
    UINT ia_offsets[] = { 0 };

    float blendFactor[4] = { 0, 0, 0, 0 };

    T::m_d3d_context->IASetInputLayout(mInputLayout);
    T::m_d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    T::m_d3d_context->IASetVertexBuffers(0, 1, ia_buffers, ia_strides, ia_offsets);
    T::m_d3d_context->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    T::m_d3d_context->VSSetShader(mVertexShader, nullptr, 0);
    T::m_d3d_context->VSSetConstantBuffers(0, 1, &mConstantBufferPerFrame);
    T::m_d3d_context->VSSetConstantBuffers(1, 1, &mConstantBufferPerDraw);
    T::m_d3d_context->GSSetShader(nullptr, nullptr, 0);
    T::m_d3d_context->RSSetState(mRasterizerState);
    T::m_d3d_context->PSSetShader(mPixelShader, nullptr, 0);
    T::m_d3d_context->PSSetSamplers(0, 1, &mSamplerState);
    T::m_d3d_context->OMSetBlendState(mBlendState, blendFactor, 0xffffffff);
    T::m_d3d_context->OMSetDepthStencilState(mDepthStencilState, 0);

    size_t xformCount = _transforms.size();
    assert(xformCount <= mObjectCount);

    // Code below assumes at least 1 texture.
    assert(mTextureSRVs.size() > 0);
    auto srvIt = mTextureSRVs.begin();

    ConstantsPerDraw cDraw;
    for (size_t u = 0; u < xformCount; ++u) {
        cDraw.World = transpose(_transforms[u]);
        T::m_d3d_context->UpdateSubresource(mConstantBufferPerDraw, 0, nullptr, &cDraw, 0, 0);

        if (srvIt == mTextureSRVs.end()) {
            srvIt = mTextureSRVs.begin();
        }

        ID3D11ShaderResourceView* activeTexSRV = *srvIt;
        ++srvIt;

        T::m_d3d_context->PSSetShaderResources(0, 1, &activeTexSRV);
        T::m_d3d_context->DrawIndexed(mIndexCount, 0, 0);
    }
}

// --------------------------------------------------------------------------------------------------------------------
template<class T>
void TexturedQuadsD3D11Naive<T>::Shutdown()
{
    for (auto it = mTextureSRVs.begin(); it != mTextureSRVs.end(); ++it) {
        SafeRelease(*it);
    }
    mTextureSRVs.clear();

    SafeRelease(mVertexBuffer);

    SafeRelease(mInputLayout);
    SafeRelease(mConstantBufferPerDraw);
    SafeRelease(mConstantBufferPerFrame);
    SafeRelease(mVertexShader);
    SafeRelease(mPixelShader);
    SafeRelease(mRasterizerState);
    SafeRelease(mBlendState);
    SafeRelease(mDepthStencilState);
    SafeRelease(mSamplerState);
}

