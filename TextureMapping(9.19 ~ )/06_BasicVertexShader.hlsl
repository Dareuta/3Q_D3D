#include "06_Shared.hlsli"

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;

    output.Pos = mul(input.Pos, World);
    output.Pos = mul(output.Pos, View);
    output.Pos = mul(output.Pos, Projection);

    output.Norm = normalize(mul(float4(input.Norm, 0), WorldInvTranspose).xyz);   
    //output.Norm = normalize(mul(float4(input.Norm, 0), World).xyz);   
    
    return output;
}