// Constant Buffers ---------------------------------------------------------------------------------------------------

Texture2D		g_Texture[2] : register(t0);
SamplerState	g_Sampler : register(s0);

cbuffer CB0 : register(b0)
{
	float4x4	World;
	uint		InstanceId;
};

// Input --------------------------------------------------------------------------------------------------------------
struct PsInput
{ 
    float4		f4Position : SV_Position;
    float2		f2TexCoord : TexCoord;
	uint		id : InstanceId;
};

// Output -------------------------------------------------------------------------------------------------------------
struct PsOutput
{
    float4 f4Color : SV_Target0;
};

// Functions ----------------------------------------------------------------------------------------------------------
PsOutput psMain(PsInput I)
{
    PsOutput O = (PsOutput)0;
	if (InstanceId % 2 == 0)
		O.f4Color = g_Texture[0].Sample(g_Sampler, I.f2TexCoord);
	else
		O.f4Color = g_Texture[1].Sample(g_Sampler, I.f2TexCoord);

    return O;
}
