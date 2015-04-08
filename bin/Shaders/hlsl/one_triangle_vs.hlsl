void main(
	in uint iVtx : SV_VertexID,
	out float4 o_pos : SV_Position)
{
	if (iVtx == 0)
	{
		o_pos = float4(-0.4, -0.6, 0, 1);
	}
	else if (iVtx == 1)
	{
		o_pos = float4(0.5, -0.1, 0, 1);
	}
	else
	{
		o_pos = float4(0.1, 0.7, 0, 1);
	}
}
