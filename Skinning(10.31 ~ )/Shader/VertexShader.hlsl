//====================== VertexShader.hlsl ======================
#include "Shared.hlsli"

#ifdef SKINNED
VS_OUT main(VS_IN_SKINNED i)
{
    // 1) 스키닝(모델 공간)
    float4 p = 0;
    float3 n = 0;
    float3 t = 0;

    [unroll]
    for (int k = 0; k < 4; ++k) {
        const uint  bi = i.Bi[k];
        const float bw = i.Bw[k];
        const float4x4 B = gBones[bi];         // (offset * nodeGlobal), C++에서 transpose된 상태

        p += bw * mul(float4(i.Pos, 1), B);
        n += bw * mul(float4(i.Nor, 0), B).xyz;
        t += bw * mul(float4(i.Tan.xyz, 0), B).xyz;
    }

    // 2) 월드/뷰/프로젝션
    float4 wp = mul(p, World);
    float3 wn = mul(float4(normalize(n), 0), WorldInvTranspose).xyz;
    float3 wt = mul(float4(normalize(t), 0), WorldInvTranspose).xyz;

    VS_OUT o;
    o.SvPos    = mul(mul(wp, View), Projection);
    o.WorldPos = wp.xyz;
    o.WorldNor = normalize(wn);
    o.WorldTan = normalize(wt);
    o.Tex      = i.Tex;
    return o;
}
#else
VS_OUT main(VS_IN_STATIC i)
{
    float4 wp = mul(float4(i.Pos, 1), World);
    float3 wn = mul(float4(i.Nor, 0), WorldInvTranspose).xyz;
    float3 wt = mul(float4(i.Tan.xyz, 0), WorldInvTranspose).xyz;

    VS_OUT o;
    o.SvPos = mul(mul(wp, View), Projection);
    o.WorldPos = wp.xyz;
    o.WorldNor = normalize(wn);
    o.WorldTan = normalize(wt);
    o.Tex = i.Tex;
    return o;
}
#endif
