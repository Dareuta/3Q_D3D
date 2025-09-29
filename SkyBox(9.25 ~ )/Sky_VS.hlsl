cbuffer CB : register(b0)
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
}
struct VS_IN
{
    float3 Pos : POSITION;
};
struct VS_OUT
{
    float4 SvPos : SV_POSITION;
    float3 Dir : TEXCOORD0;
};
VS_OUT main(VS_IN i)
{
    float4 w = mul(float4(i.Pos, 1.0f), World);
    VS_OUT o;
    o.SvPos = mul(mul(w, View), Projection);
    o.Dir = w.xyz;
    return o;
}
