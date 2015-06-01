#pragma once

#include "solutions/texturedquadssoln.h"
#include "framework/gfx_dx11on12.h"

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
template<class T>
class TexturedQuadsD3D11Naive : public TexturedQuadsSolution
{
public:
    TexturedQuadsD3D11Naive();
    virtual ~TexturedQuadsD3D11Naive() { }

    virtual bool Init(const std::vector<TexturedQuadsProblem::Vertex>& _vertices,
                      const std::vector<TexturedQuadsProblem::Index>& _indices,
                      const std::vector<TextureDetails*>& _textures,
                      size_t _objectCount);

    virtual void Render(const std::vector<Matrix>& _transforms);
    virtual void Shutdown();

    virtual std::string GetName() const { return "D3D11Naive"; }
	virtual bool SupportsApi(EGfxApi _api) const override { return T::GetApiTypeStatic() == _api; }

private:
    struct ConstantsPerDraw
    {
        Matrix World;
    };

    struct ConstantsPerFrame
    {
        Matrix ViewProjection;
    };

    ID3D11InputLayout* mInputLayout;
    ID3D11Buffer* mConstantBufferPerFrame;
    ID3D11Buffer* mConstantBufferPerDraw;
    ID3D11VertexShader* mVertexShader;
    ID3D11PixelShader* mPixelShader;
    ID3D11SamplerState* mSamplerState;
    ID3D11RasterizerState* mRasterizerState;
    ID3D11BlendState* mBlendState;
    ID3D11DepthStencilState* mDepthStencilState;

    ID3D11Buffer* mVertexBuffer;
    ID3D11Buffer* mIndexBuffer;

    std::vector<ID3D11ShaderResourceView*> mTextureSRVs;
};
