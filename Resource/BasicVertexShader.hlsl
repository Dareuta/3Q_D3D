#include "07_0_Shared.hlsli"

PS_INPUT main(VS_INPUT i)
{
    PS_INPUT o;

    // World space position
    float4 wpos = mul(float4(i.Pos, 1.0f), World);

    // Clip space
    o.PosH = mul(mul(wpos, View), Projection);

    // Pass to PS
    o.WorldPos = wpos.xyz;
    o.Tex = i.Tex;

    // 방향벡터는 WorldInvTranspose로 변환(스케일 보정) 후 정규화
    o.NormalW = normalize(mul(float4(i.Norm, 0.0f), WorldInvTranspose).xyz);
    o.TangentW = normalize(mul(float4(i.Tang, 0.0f), WorldInvTranspose).xyz);
    o.BitangentW = normalize(mul(float4(i.Bitan, 0.0f), WorldInvTranspose).xyz);

    return o;
}
