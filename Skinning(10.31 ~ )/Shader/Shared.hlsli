//====================== Shared.hlsli ======================
// 공용 상수버퍼 (너의 기존 레이아웃 유지)
cbuffer CB0 : register(b0)
{
    float4x4 World, View, Projection, WorldInvTranspose;
    float4 vLightDir;
    float4 vLightColor;
}
cbuffer BP : register(b1)
{
    float4 EyePosW; // (ex,ey,ez,1)
    float4 kA; // ambient reflectance
    float4 kSAlpha; // x: ks, y: shininess (or alphaCut), z,w: unused
    float4 I_ambient; // ambient light
}
cbuffer USE : register(b2)
{
    uint useDiffuse, useNormal, useSpecular, useEmissive;
    uint useOpacity;
    float alphaCut;
    float2 _padUSE;
}

// ★ 스키닝 본 팔레트 (VS에서만 사용) — C++에서 Transpose 해서 올릴 것!
cbuffer BONES : register(b4)
{
    float4x4 gBones[256];
}

// ===== 정점 입력 (정적 / 스키닝) =====
struct VS_IN_STATIC
{
    float3 Pos : POSITION;
    float3 Nor : NORMAL;
    float2 Tex : TEXCOORD;
    float4 Tan : TANGENT; // xyz: tangent, w: handedness
};

struct VS_IN_SKINNED
{
    float3 Pos : POSITION;
    float3 Nor : NORMAL;
    float2 Tex : TEXCOORD;
    float4 Tan : TANGENT;
    uint4 Bi : BLENDINDICES; // R8G8B8A8_UINT
    float4 Bw : BLENDWEIGHT; // float4
};

// ===== VS → PS 공용 출력 =====
struct VS_OUT
{
    float4 SvPos : SV_Position;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNor : TEXCOORD1;
    float3 WorldTan : TEXCOORD2;
    float2 Tex : TEXCOORD3;
};

// (PixelShader가 PS_INPUT 이름을 기대한다면 호환을 위해 동일 구조체를 한 번 더 선언)
struct PS_INPUT
{
    float4 SvPos : SV_Position;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNor : TEXCOORD1;
    float3 WorldTan : TEXCOORD2;
    float2 Tex : TEXCOORD3;
};
