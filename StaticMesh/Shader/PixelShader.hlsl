#include "Shared.hlsli"

#ifndef NORMALMAP_FLIP_GREEN   
#define NORMALMAP_FLIP_GREEN 0 
#endif                         

#ifndef SWAPCHAIN_SRGB
#define SWAPCHAIN_SRGB 1   // sRGB 스왑체인 쓰면 1, 아니면 0
#endif

float4 main(PS_INPUT input) : SV_Target
{
    AlphaClip(input.Tex); // useOpacity/alphaCut/txOpacity에 따라 clip
    //===============================================================
    float3 albedo = (useDiffuse != 0) ?
        txDiffuse.Sample(samLinear, input.Tex).rgb : float3(1, 1, 1);
    float specMask = (useSpecular != 0) ?
        txSpecular.Sample(samLinear, input.Tex).r : 1.0f;
    float3 emissive = (useEmissive != 0) ?
        txEmissive.Sample(samLinear, input.Tex).rgb : float3(0, 0, 0);
    //===============================================================
    
    float3 Nw_base = normalize(input.NormalW);
    float3 Tw = OrthonormalizeTangent(Nw_base, input.TangentW.xyz);
    float3 Nw = (useNormal != 0) ?
        ApplyNormalMapTS(Nw_base, Tw, input.TangentW.w, input.Tex, NORMALMAP_FLIP_GREEN) : Nw_base;

    //===============================================================
           
    float3 L = normalize(-vLightDir.xyz);
    float3 V = normalize(EyePosW.xyz - input.WorldPos);
    float3 H = normalize(L + V);

    float ks = kSAlpha.x; // x값에 ks
    float shin = max(1.0f, kSAlpha.y); // y값에 alpha(shininess) 들어감 - 최소 1
    
    float NdotL = saturate(dot(Nw, L)); // 빛벡터와 법선을 내적
    float3 diff = albedo * NdotL; // 기본 베이스 색 * 0 ~ 1    
  
    float3 spec = specMask * ks * pow(saturate(dot(Nw, H)), shin);
    float3 ambient = I_ambient.rgb * kA.rgb * albedo;
    
    float3 color = ambient + emissive + vLightColor.rgb * (diff + spec);
   
#if SWAPCHAIN_SRGB
    return float4(color, 1.0f);
#else
    float3 color_srgb = pow(saturate(color), 1.0 / 2.2);
    return float4( color_srgb, 1.0f );
#endif
}
