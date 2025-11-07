#ifndef SHARED_HLSLI_INCLUDED
#define SHARED_HLSLI_INCLUDED


cbuffer MAT : register(b5)
{
    float4 matBaseColor;
    uint matUseBaseColor;
    uint3 _matPad5;
}


// ===== CB0 (b0)
cbuffer CB0 : register(b0)
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
    float4x4 WorldInvTranspose;
    float4 vLightDir;
    float4 vLightColor;
}

// ===== BP (b1)
cbuffer BP : register(b1)
{
    float4 EyePosW;
    float4 kA;
    float4 kSAlpha; // x: ks, y: shininess/alphaCut
    float4 I_ambient;
}

// ===== USE (b2)
cbuffer USE : register(b2)
{
    uint useDiffuse;
    uint useNormal;
    uint useSpecular;
    uint useEmissive;
    uint useOpacity;
    float alphaCut;
    float2 _pad;
}

// ===== Textures / Sampler
Texture2D txDiffuse : register(t0);
Texture2D txNormal : register(t1);
Texture2D txSpecular : register(t2);
Texture2D txEmissive : register(t3);
Texture2D txOpacity : register(t4);
SamplerState samLinear : register(s0);

// ===== VS input (단일 구조체)
struct VS_INPUT
{
    float3 Pos : POSITION;
    float3 Norm : NORMAL;
    float2 Tex : TEXCOORD0;
    float4 Tang : TANGENT; // (= TANGENT0)

#if defined(SKINNED)
    uint4  BlendIndices : BLENDINDICES;
    float4 BlendWeights : BLENDWEIGHT;
#endif
};

// ===== PS input
struct PS_INPUT
{
    float4 PosH : SV_Position;
    float3 WorldPos : TEXCOORD0;
    float2 Tex : TEXCOORD1;
    float4 TangentW : TEXCOORD2;
    float3 NormalW : TEXCOORD3;
};

// ===== Shadow Map (PS: t5/s1, CB: b6) =====
Texture2D<float> txShadow : register(t5);

// 깊이 비교 전용 샘플러(하드웨어 PCF 사용 가능)
SamplerComparisonState samShadow : register(s1);

cbuffer ShadowCB : register(b6)
{
    float4x4 LightViewProj; // 라이트 뷰-프로젝션 (C++에서 Transpose 해서 넣기)
    float4 ShadowParams; // x: depthBias, y: 1/texW, z: 1/texH, w: 예약
}

// ===== Helpers
inline float3 OrthonormalizeTangent(float3 N, float3 T)
{
    T = normalize(T - N * dot(T, N));
    float3 B = normalize(cross(N, T));
    T = normalize(cross(B, N));
    return T;
}

inline float3 ApplyNormalMapTS(float3 Nw, float3 Tw, float sign, float2 uv, int flipGreen)
{
    float3 Bw = normalize(cross(Nw, Tw)) * sign;
    Tw = normalize(cross(Bw, Nw));
    float3x3 TBN = float3x3(Tw, Bw, Nw);
    float3 nTS = txNormal.Sample(samLinear, uv).xyz * 2.0f - 1.0f;
    if (flipGreen)
        nTS.g = -nTS.g;
    return normalize(mul(nTS, TBN));
}

inline void AlphaClip(float2 uv)
{
    if (useOpacity != 0)
    {
        float a = txOpacity.Sample(samLinear, uv).r;
        clip(a - alphaCut);
    }
}

// ==== Shadow sampling (PCF 3x3) ====
float SampleShadow_PCF(float3 worldPos, float3 Nw)
{
    // 월드 → 라이트 clip
    float4 Lc = mul(float4(worldPos, 1.0f), LightViewProj);
    float w = max(1e-6, Lc.w);
    float3 ndc = Lc.xyz / w;

    // 라이트 NDC → UV
    float2 uv = ndc.xy * 0.5f + 0.5f;
    if (any(uv < 0.0) || any(uv > 1.0) || ndc.z < 0.0 || ndc.z > 1.0)
        return 1.0; // 밖이면 그림자 없음

    // 바이어스(법선 기반 보정)
    float bias = ShadowParams.x;
    float3 L = normalize(-vLightDir.xyz);
    bias *= (1.0 - 0.5 * saturate(dot(Nw, L)));

    // 3x3 PCF
    float2 duv = ShadowParams.yz; // (1/texW, 1/texH)
    float s = 0.0;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 o = float2(x, y) * duv;
            s += txShadow.SampleCmpLevelZero(samShadow, uv + o, ndc.z - bias);
        }
    return s / 9.0;
}

// ===== Bones (b4) — Shared에만 둔다!
static const uint kMaxBones = 256;
#if defined(SKINNED)
cbuffer Bones : register(b4)
{
    float4x4 BonePalette[kMaxBones];
}
#endif

#endif // SHARED_HLSLI_INCLUDED
