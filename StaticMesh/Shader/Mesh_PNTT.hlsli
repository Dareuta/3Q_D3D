#ifndef MESH_PNTT_HLSLI
#define MESH_PNTT_HLSLI

// ===== 슬롯 매크로 =====
#define CB_SLOT_GLOBAL   b0
#define CB_SLOT_MATERIAL b1
#define CB_SLOT_USEFLAGS b2
#define TEX_DIFFUSE      t0
#define TEX_NORMAL       t1
#define TEX_SPECULAR     t2
#define TEX_EMISSIVE     t3
#define TEX_OPACITY      t4
#define SAMP_LINEAR      s0

// ===== 상수 버퍼 =====
cbuffer CB0 : register(CB_SLOT_GLOBAL) {
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
    float4x4 WorldInvTranspose;
    float4   vLightDir;    // 컨벤션: 광원→표면. (PS에서 L 만들 때 -붙임)
    float4   vLightColor;  // rgb * intensity
};

cbuffer BP : register(CB_SLOT_MATERIAL) {
    float4 EyePosW;     // (ex,ey,ez,1)
    float4 kA;          // ambient reflectance (rgb)
    float4 kSAlpha;     // x: ks, y: shininess/alphaCut
    float4 I_ambient;   // ambient light (rgb)
};

cbuffer USE : register(CB_SLOT_USEFLAGS) {
    uint  useDiffuse, useNormal, useSpecular, useEmissive;
    uint  useOpacity; float alphaCut; float2 _pad;
};

// ===== IO 구조체 =====
struct VS_IN  {
    float3 Pos : POSITION;
    float3 Nor : NORMAL;
    float2 Tex : TEXCOORD0;
    float4 Tan : TANGENT;   // xyz=tangent, w=bitangent sign(+1/-1)
};

struct VS_OUT {
    float4 Pos       : SV_Position;
    float3 WorldPos  : TEXCOORD0;
    float3 NorW      : TEXCOORD1;
    float2 Tex       : TEXCOORD2;
    float3 TanW      : TEXCOORD3;
    float  TanWSign  : TEXCOORD4;
};

// ===== 리소스 =====
Texture2D    txDiffuse  : register(TEX_DIFFUSE);
Texture2D    txNormal   : register(TEX_NORMAL);
Texture2D    txSpecular : register(TEX_SPECULAR);
Texture2D    txEmissive : register(TEX_EMISSIVE);
Texture2D    txOpacity  : register(TEX_OPACITY);
SamplerState samp       : register(SAMP_LINEAR);

// ===== 유틸 =====
inline float3 OrthonormalizeTangent(float3 N, float3 T)
{
    T = normalize(T - N * dot(T, N));    // Gram–Schmidt
    float3 B = normalize(cross(N, T));
    T = normalize(cross(B, N));          // 수치 안정화
    return T;
}

inline float3 ApplyNormalMapTS(float3 N, float3 T, float sign, float2 uv)
{
    float3 B = normalize(cross(N, T)) * sign;
    T = normalize(cross(B, N));                 // 재직교화
    float3x3 TBN = float3x3(T, B, N);           // 행벡터 기준 mul(nTS, TBN)
    float3 nTS = txNormal.Sample(samp, uv).xyz * 2.0 - 1.0;
    return normalize(mul(nTS, TBN));
}

#endif // MESH_PNTT_HLSLI
