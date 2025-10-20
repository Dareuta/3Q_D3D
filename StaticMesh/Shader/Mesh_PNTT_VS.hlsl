cbuffer CB0 : register(b0) {
    float4x4 World, View, Projection, WorldInvTranspose;
    float4   vLightDir;   // xyz (정규화), w 미사용
    float4   vLightColor; // rgb*intensity
}
struct VS_IN  { float3 Pos:POSITION; float3 Nor:NORMAL; float2 Tex:TEXCOORD0; float4 Tan:TANGENT; };
struct VS_OUT {
    float4 Pos:SV_Position;
    float3 WorldPos: TEXCOORD0;
    float3 NorW   : TEXCOORD1;
    float2 Tex    : TEXCOORD2;
    float3 TanW   : TEXCOORD3;
    float  TanWSign: TEXCOORD4;
};
VS_OUT main(VS_IN i) {
    VS_OUT o;
    float4 wpos = mul(float4(i.Pos,1), World);
    o.Pos      = mul(mul(wpos, View), Projection);
    o.WorldPos = wpos.xyz;
    o.NorW     = normalize(mul(float4(i.Nor,0), WorldInvTranspose).xyz);
    float3 T   = normalize(mul(float4(i.Tan.xyz,0), World).xyz);
    o.TanW     = T;  o.TanWSign = i.Tan.w;
    o.Tex      = i.Tex;
    return o;
}
