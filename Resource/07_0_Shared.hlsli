#ifndef SHARED_HLSLI_INCLUDED
#define SHARED_HLSLI_INCLUDED

// ===== 상수버퍼 (C++의 ConstantBuffer와 BlinnPhongCB에 정확히 대응) =====
cbuffer CB0 : register(b0)
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
    float4x4 WorldInvTranspose;

    float4 vLightDir[2];
    float4 vLightColor[2];
    float4 vOutputColor;
}

cbuffer BP : register(b1)
{
    float4 EyePosW; // (ex,ey,ez,1)
    float4 kA; // (ka.r,ka.g,ka.b,0)
    float4 kSAlpha; // (ks, alpha, 0, 0)
    float4 I_ambient; // (Ia.r,Ia.g,Ia.b,0)
}

// ===== 텍스처 & 샘플러 =====
Texture2D txDiffuse : register(t0); // sRGB 원본(로더에서 선형화)
Texture2D txNormal : register(t1); // 선형(절대 sRGB 금지)
Texture2D txSpecular : register(t2); // 회색 강도맵(보통 선형)

// 감마 고려는 로더에서 처리했으니 여기선 그냥 샘플하면 선형공간임.
SamplerState samLinear : register(s0);

// ===== VS/PS 인터페이스 =====
struct VS_INPUT
{
    float3 Pos : POSITION;
    float3 Norm : NORMAL;
    float2 Tex : TEXCOORD0;
    float3 Tang : TEXCOORD1; // +U
    float3 Bitan : TEXCOORD2; // +V
};

struct PS_INPUT
{
    float4 PosH : SV_POSITION; // homogeneous clip pos
    float3 WorldPos : TEXCOORD0;
    float2 Tex : TEXCOORD1;
    float3 TangentW : TEXCOORD2;
    float3 BitangentW : TEXCOORD3;
    float3 NormalW : TEXCOORD4;
};

#endif // SHARED_HLSLI_INCLUDED
