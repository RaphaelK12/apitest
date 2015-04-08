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
	
	matrix m = { -1.6, 0.8, 0.0, 0.0,
		0.7, 1.4, 1.79, 0.0,
		-0.33, -0.677, 0.667, 299.0,
		-0.33, -0.67, 0.67, 300.0 };
	//O.f4Position = mul(m , float4(I.f3Position, 1.0f) );// mul(mul(float4(I.f3Position, 1.0f), World), ViewProjection);
	//O.f4Position = mul(mul(float4(I.f3Position, 1.0f), World), ViewProjection);
	O.f4Position = mul(float4(I.f3Position, 1.0f), ViewProjection);
    O.f3Color = I.f3Color;

    return O;
}
