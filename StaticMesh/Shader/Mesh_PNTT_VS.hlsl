#include "Mesh_PNTT.hlsli"

VS_OUT main(VS_IN i)
{
    VS_OUT o;

    float4 wpos = mul(float4(i.Pos, 1.0), World);
    o.Pos       = mul(mul(wpos, View), Projection);
    o.WorldPos  = wpos.xyz;

    float3x3 W3   = (float3x3)World;
    float3x3 WIT3 = (float3x3)WorldInvTranspose;

    float3 N = normalize(mul(i.Nor, WIT3));
    float3 T = normalize(mul(i.Tan.xyz, W3));
    T = OrthonormalizeTangent(N, T);

    o.NorW      = N;
    o.TanW      = T;
    o.TanWSign  = i.Tan.w;
    o.Tex       = i.Tex;

    return o;
}
