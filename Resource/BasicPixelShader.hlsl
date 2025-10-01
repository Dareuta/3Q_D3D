#include "07_0_Shared.hlsli"

// 필요 시 노말맵 그린 채널 뒤집기(엔진/아트 파이프라인에 따라)
#ifndef NORMALMAP_FLIP_GREEN
#define NORMALMAP_FLIP_GREEN 0
#endif

float4 main(PS_INPUT input) : SV_Target
{
    // Albedo (이미 선형공간)
    float3 albedo = txDiffuse.Sample(samLinear, input.Tex).rgb;

    // Tangent-space normal: [0,1]->[-1,1]
    float3 n_t = txNormal.Sample(samLinear, input.Tex).xyz * 2.0f - 1.0f;
#if NORMALMAP_FLIP_GREEN
    n_t.y = -n_t.y;
#endif
    n_t = normalize(n_t);

    // World-space TBN
    float3 T = normalize(input.TangentW);
    float3 B = normalize(input.BitangentW);
    float3 N = normalize(input.NormalW);

    // 보정: 수치오차로 T,B가 N과 비직교면 Gram-Schmidt로 조금 정리
    T = normalize(T - N * dot(T, N));
    B = normalize(cross(N, T)); // 오른손계 유지

    float3x3 TBN = float3x3(T, B, N);

    // 최종 노말(월드공간)
    float3 Nw = normalize(mul(n_t, TBN));

    // 조명 벡터들
    float3 L = normalize(-vLightDir[0].xyz); // 빛이 오는 방향
    float3 V = normalize(EyePosW.xyz - input.WorldPos);
    float3 H = normalize(L + V);

    // 파라미터
    float ks = kSAlpha.x;
    float shin = max(1.0f, kSAlpha.y);

    // 디퓨즈
    float NdotL = saturate(dot(Nw, L));
    float3 diff = albedo * NdotL;

    // 스펙: 텍스처(회색 강도 맵) * ks * (N·H)^shin
    float specTex = txSpecular.Sample(samLinear, input.Tex).r; // 회색맵이면 R 채널만
    float3 spec = specTex * ks * pow(saturate(dot(Nw, H)), shin);

    // 앰비언트
    float3 ambient = I_ambient.rgb * kA.rgb * albedo;

    // 라이트 컬러 적용(0번 광원만 사용)
    float3 color = ambient + vLightColor[0].rgb * (diff + spec);

    return float4(color, 1.0f);
}
