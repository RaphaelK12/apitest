// Constant Buffers ---------------------------------------------------------------------------------------------------

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
	if (I.id % 2 == 0)
		O.f4Color = float4(1.0, 0.0, 0.0, 1.0);
	else
		O.f4Color = float4(0.0, 0.0, 1.0, 1.0);

    return O;
}
