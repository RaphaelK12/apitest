// Constant Buffers ---------------------------------------------------------------------------------------------------
cbuffer CB0 : register(b0)
{
	float4x4	World;
	uint		InstanceId;
};

cbuffer CB1 : register(b1)
{
	float4x4 ViewProjection;
};

// Input --------------------------------------------------------------------------------------------------------------
struct VsInput
{
    float3	f3Position	: ObjPos;
    float2	f2TexCoord	: TexCoord;
	uint	id			: SV_InstanceID;
};

// Output -------------------------------------------------------------------------------------------------------------
struct VsOutput 
{
    float4	f4Position : SV_Position;
    float2	f2TexCoord : TexCoord;
	uint	id : InstanceId;
};

// Functions ----------------------------------------------------------------------------------------------------------
VsOutput vsMain(VsInput I)
{
    VsOutput O = (VsOutput) 0;
	O.f4Position = mul(mul(World, float4(I.f3Position, 1.0f)), ViewProjection);
    O.f2TexCoord = I.f2TexCoord;
	O.id = InstanceId;

    return O;
}
