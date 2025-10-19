cbuffer CB : register(b0)
{
    float4x4 gWorld, gView, gProj;
};
struct VSIn
{
    float3 pos : POSITION;
};
struct VSOut
{
    float4 svpos : SV_POSITION;
};
VSOut main(VSIn i)
{
    VSOut o;
    float4 wp = mul(float4(i.pos, 1), gWorld);
    o.svpos = mul(mul(wp, gView), gProj);
    return o;
}
