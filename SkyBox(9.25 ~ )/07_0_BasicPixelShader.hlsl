#include "07_0_Shared.hlsli"

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 main(PS_INPUT input) : SV_Target
{
    float4 finalColor = 0;
    float4 albedo =  txDiffuse.Sample(samLinear, input.Tex);        

    //do NdotL lighting for 2 lights
    for (int i = 0; i < 2; i++)
    {
        finalColor += saturate(dot((float3) vLightDir[i], input.Norm) * vLightColor[i]);
    }
    finalColor.a = 1;

    return finalColor * albedo;
}