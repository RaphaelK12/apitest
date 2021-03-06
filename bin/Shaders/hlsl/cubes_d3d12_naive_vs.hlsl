// Constant Buffers ---------------------------------------------------------------------------------------------------
cbuffer Constants : register(b0)
{
	matrix World;
};

cbuffer Constants : register(b1)
{
	matrix ViewProjection;
};

// Input --------------------------------------------------------------------------------------------------------------
struct VsInput
{
    float3 f3Position   : ObjPos;
    float3 f3Color      : Color;
	uint	id			: SV_InstanceID;
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
	O.f4Position = mul(mul(World, float4(I.f3Position, 1.0f)), ViewProjection);
    O.f3Color = I.f3Color;

    return O;
}
