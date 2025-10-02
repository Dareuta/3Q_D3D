#include "07_0_Shared.hlsli"

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------



PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;


    float4 wpos = mul(input.Pos, World);
    output.WorldPos = wpos.xyz;
    
    float4 vpos = mul(wpos, View);
    output.Pos = mul(vpos, Projection);
                
    output.Tex = input.Tex;


    output.Norm = normalize(mul(float4(input.Norm, 0), WorldInvTranspose).xyz);
    //output.Norm = normalize(mul(float4(input.Norm, 0), World).xyz);   
    
    return output;
}