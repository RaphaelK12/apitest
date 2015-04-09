// Constant Buffers ---------------------------------------------------------------------------------------------------
cbuffer Constants : register(b0)
{
 //   matrix ViewProjection;
	matrix World;
};

cbuffer Constants : register(b1)
{
	matrix ViewProjection1;
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

	//O.f4Position = mul(mul(World, float4(I.f3Position + 62.0f, 1.0f)), ViewProjection1);
	//O.f4Position = mul(float4(I.f3Position + 50.0f, 1.0f), ViewProjection1);
//	O.f4Position = float4(I.f3Position, 1.0f);
	O.f4Position = mul(mul(float4(I.f3Position, 1.0f), World), ViewProjection1);
	//O.f4Position = mul(float4(I.f3Position + 50.0f, 1.0f), ViewProjection1);
    O.f3Color = I.f3Color;

    return O;
}
