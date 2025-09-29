#include "07_0_Shared.hlsli"

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 main(PS_INPUT input) : SV_Target
{
        // k_d (알베도)
    float3 kd = txDiffuse.Sample(samLinear, input.Tex).rgb;

    // N, L, V, H
    float3 N = normalize(input.Norm);
    float3 L = normalize(-vLightDir[0].xyz); // 보통 “빛이 오는 방향”이므로 -dir
    float3 V = normalize(EyePosW.xyz - input.WorldPos); // 카메라 → 표면
    float3 H = normalize(L + V); // Half vector

    // 파라미터
    float3 Ia = I_ambient.rgb; // 환경광
    float3 Il = vLightColor[0].rgb; // 직접광 (색*강도는 C++ 쪽에서 이미 곱해서 넘겼음)
    float3 ka = kA.rgb; // 재질 ka
    float ks = kSAlpha.x; // 재질 ks
    float a = max(1.0f, kSAlpha.y); // shininess (>= 1)

    // 항 계산
    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));

    float3 ambient = Ia * ka * kd;
    float3 diffuse = Il * kd * NdotL;
    float3 specular = Il * ks * pow(NdotH, a);
    //specular *= step(0.0f, NdotL); // 뒷면 스펙큘러 방지용이라는데, 고민해봐야할듯

    float3 color = ambient + diffuse + specular;
    return float4(saturate(color), 1.0f);
}