cbuffer CB0 : register(b0)
{
    float4x4 World, View, Projection, WorldInvTranspose;
    float4 vLightDir; // 표면→광원 방향을 넣을거면 뒤에서 -하지 말 것. (아래 PS 주석 참조)
    float4 vLightColor; // rgb*intensity
}

struct VS_IN
{
    float3 Pos : POSITION;
    float3 Nor : NORMAL;
    float2 Tex : TEXCOORD0;
    float4 Tan : TANGENT;
};
struct VS_OUT
{
    float4 Pos : SV_Position;
    float3 WorldPos : TEXCOORD0;
    float3 NorW : TEXCOORD1;
    float2 Tex : TEXCOORD2;
    float3 TanW : TEXCOORD3;
    float TanWSign : TEXCOORD4;
};

VS_OUT main(VS_IN i)
{
    VS_OUT o;

    float4 wpos = mul(float4(i.Pos, 1), World);
    o.Pos = mul(mul(wpos, View), Projection);
    o.WorldPos = wpos.xyz;

    // N: WorldInvTranspose, T: World(3x3) → 직교화
    float3x3 W3 = (float3x3) World;
    float3x3 WIT3 = (float3x3) WorldInvTranspose;

    float3 N = normalize(mul(i.Nor, WIT3));
    float3 T = normalize(mul(i.Tan.xyz, W3));

    // Gram-Schmidt: T를 N에 직교화
    T = normalize(T - N * dot(T, N));

    // (선택) B를 한 번 만들어 직교 상태 확인 후 T 재보정
    float3 B = normalize(cross(N, T)) * i.Tan.w;
    T = normalize(cross(B, N)); // 수치 누적 방지

    o.NorW = N;
    o.TanW = T;
    o.TanWSign = i.Tan.w;
    o.Tex = i.Tex;
    return o;
}
