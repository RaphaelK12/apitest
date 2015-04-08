// Constant Buffers ---------------------------------------------------------------------------------------------------
cbuffer Constants : register(b0)
{
    matrix ViewProjection;
	matrix World;
};

// Input --------------------------------------------------------------------------------------------------------------
struct VsInput
{
    float3 f3Position   : ObjPos;
    float3 f3Color      : Color;
};

// Output -------------------------------------------------------------------------------------------------------------
struct VsOutput 
{
    float4 f4Position   : SV_Position;
    float3 f3Color      : Color;
};

// Functions ----------------------------------------------------------------------------------------------------------
VsOutput vsMain(VsInput I)
{
    VsOutput O = (VsOutput) 0;

	O.f4Position = mul(mul(float4(I.f3Position, 1.0f), World), ViewProjection);
	//O.f4Position = float4(I.f3Position, 1.0f);
    O.f3Color = I.f3Color;

    return O;
}
