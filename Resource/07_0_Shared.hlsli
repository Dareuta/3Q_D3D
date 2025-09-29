//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    matrix WorldInvTranspose;
    
    float4 vLightDir[2];
    float4 vLightColor[2];
    float4 vOutputColor;
}

cbuffer CB_BlinnPhong : register(b1)
{
    float4 EyePosW;     // 카메라 월드 좌표 (ex,ey,ez,1)
    float4 kA;          // 재질 ka = (r,g,b,0)
    float4 kSAlpha;     // (ks, alpha, 0, 0)
    float4 I_ambient;   // 환경광 Ia = (r,g,b,0)
}

//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Norm : NORMAL;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 Norm : TEXCOORD1;
    float2 Tex : TEXCOORD0;
    float3 WorldPos : TEXCOORD2;
};
