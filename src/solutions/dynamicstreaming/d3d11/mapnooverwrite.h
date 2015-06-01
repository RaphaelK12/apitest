#pragma once

#include "solutions/dynamicstreamingsoln.h"
#include "framework/gfx_dx11.h"
#include "framework/gfx_dx11on12.h"

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
template< class T >
class DynamicStreamingD3D11MapNoOverwrite : public DynamicStreamingSolution
{
public:
    DynamicStreamingD3D11MapNoOverwrite();
    virtual ~DynamicStreamingD3D11MapNoOverwrite();

    virtual bool Init(size_t _maxVertexCount) override;
    virtual void Render(const std::vector<Vec2>& _vertices) override;
    virtual void Shutdown() override;

    virtual std::string GetName() const override { return "D3D11MapNoOverwrite"; }
	virtual bool SupportsApi(EGfxApi _api) const override { return T::GetApiTypeStatic() == _api; }

private:
    struct Constants
    {
        float width;
        float height;
        float pad[2];
    };

    ID3D11InputLayout* mInputLayout;
    ID3D11Buffer* mConstantBuffer;
    ID3D11VertexShader* mVertexShader;
    ID3D11PixelShader* mPixelShader;
    ID3D11SamplerState* mSamplerState;
    ID3D11RasterizerState* mRasterizerState;
    ID3D11BlendState* mBlendState;
    ID3D11DepthStencilState* mDepthStencilState;

    ID3D11Buffer* mDynamicVertexBuffer;

    size_t mParticleBufferSize;
    size_t mStartDestOffset;
};
