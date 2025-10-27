#include "Shared.hlsli"

#ifndef NORMALMAP_FLIP_GREEN
#define NORMALMAP_FLIP_GREEN 0
#endif

#ifndef SWAPCHAIN_SRGB
#define SWAPCHAIN_SRGB 1   // sRGB 스왑체인 쓰면 1
#endif

float4 main(PS_INPUT input) : SV_Target
{
    // 0) 알파값 결정 (opacity 텍스처 있으면 사용)
    float a = 1.0f;
    if (useOpacity != 0)
    {
        a = txOpacity.Sample(samLinear, input.Tex).r;
    }

    // 1) 컷아웃 패스면 clip, 블렌드 패스면 alphaCut=-1이라 통과됨
    //    (CPU에서: cutoutPass -> alphaCut=0.5f, blendPass -> alphaCut=-1.0f)
    AlphaClip(input.Tex);

    // 2) 셰이딩(네 코드 유지)
    float3 albedo = (useDiffuse != 0) ?
        txDiffuse.Sample(samLinear, input.Tex).rgb : float3(1, 1, 1);
    float specMask = (useSpecular != 0) ?
        txSpecular.Sample(samLinear, input.Tex).r : 1.0f;
    float3 emissive = (useEmissive != 0) ?
        txEmissive.Sample(samLinear, input.Tex).rgb : float3(0, 0, 0);

    float3 Nw_base = normalize(input.NormalW);
    float3 Tw = OrthonormalizeTangent(Nw_base, input.TangentW.xyz);
    float3 Nw = (useNormal != 0) ?
        ApplyNormalMapTS(Nw_base, Tw, input.TangentW.w, input.Tex, NORMALMAP_FLIP_GREEN) : Nw_base;

    float3 L = normalize(-vLightDir.xyz);
    float3 V = normalize(EyePosW.xyz - input.WorldPos);
    float3 H = normalize(L + V);

    float ks   = kSAlpha.x;
    float shin = max(1.0f, kSAlpha.y);

    float  NdotL = saturate(dot(Nw, L));
    float3 diff  = albedo * NdotL;
    float3 spec  = specMask * ks * pow(saturate(dot(Nw, H)), shin);
    float3 amb   = I_ambient.rgb * kA.rgb * albedo;

    float3 color = amb + emissive + vLightColor.rgb * (diff + spec);

#if SWAPCHAIN_SRGB
    return float4(color, a);
#else
    float3 color_srgb = pow(saturate(color), 1.0 / 2.2);
    return float4(color_srgb, a);
#endif
}
