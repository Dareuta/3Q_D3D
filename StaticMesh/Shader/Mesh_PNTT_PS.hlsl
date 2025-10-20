#include "Mesh_PNTT.hlsli"

float4 main(VS_OUT i) : SV_Target
{
    float3 N = normalize(i.NorW);
    if (useNormal == 1)
        N = ApplyNormalMapTS(N, normalize(i.TanW), i.TanWSign, i.Tex);

    float4 albedo = (useDiffuse==1) ? txDiffuse.Sample(samp, i.Tex) : float4(1,1,1,1);
    float  alpha  = (useOpacity==1) ? txOpacity.Sample(samp, i.Tex).r : albedo.a;

    // Cutout (alpha test)
    if (alphaCut >= 0.0f) clip(alpha - alphaCut);

    // Blinn–Phong
    float3 L = normalize(-vLightDir.xyz);               // 컨벤션에 따라 -붙임
    float3 V = normalize(EyePosW.xyz - i.WorldPos);
    float3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float ks   = kSAlpha.x;
    float shin = max(kSAlpha.y, 2.0);
    float spec = (diff > 0.0) ? pow(max(dot(N, H), 0.0), shin) * ks : 0.0;

    if (useSpecular == 1)
        spec *= txSpecular.Sample(samp, i.Tex).r;

    float3 ambient = I_ambient.rgb * kA.rgb * albedo.rgb;
    float3 lit     = ambient + vLightColor.rgb * (albedo.rgb * diff + spec);

    if (useEmissive == 1)
        lit += txEmissive.Sample(samp, i.Tex).rgb;

    return float4(lit, 1.0 /* 투명 블렌딩 쓸거면 alpha */);
}
