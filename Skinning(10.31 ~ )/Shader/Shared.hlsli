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

// ===== Bones (b4) — Shared에만 둔다!
static const uint kMaxBones = 256;
#if defined(SKINNED)
cbuffer Bones : register(b4)
{
    float4x4 BonePalette[kMaxBones];
}
#endif

#endif // SHARED_HLSLI_INCLUDED
