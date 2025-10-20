Texture2D txDiffuse  : register(t0);
Texture2D txNormal   : register(t1);
Texture2D txSpecular : register(t2);
Texture2D txEmissive : register(t3);
Texture2D txOpacity  : register(t4);
SamplerState samp    : register(s0);

cbuffer CB0 : register(b0) {
    float4x4 World, View, Projection, WorldInvTranspose;
    float4   vLightDir;
    float4   vLightColor;
};
cbuffer BP  : register(b1) { // 재질/카메라
    float4 EyePosW;
    float4 kA;        // ambient reflectance
    float4 kSAlpha;   // x: ks, y: shininess/alphaCut, z,w 미사용
    float4 I_ambient; // ambient light
};
cbuffer USE : register(b2) { // 사용 여부
    uint useDiffuse, useNormal, useSpecular, useEmissive;
    uint useOpacity;  float alphaCut; float2 _pad;
}

struct VS_OUT {
    float4 Pos:SV_Position;
    float3 WorldPos: TEXCOORD0;
    float3 NorW   : TEXCOORD1;
    float2 Tex    : TEXCOORD2;
    float3 TanW   : TEXCOORD3;
    float  TanWSign: TEXCOORD4;
};

float3 ApplyNormalMap(float3 N, float3 T, float sign, float2 uv) {
    float3 B = normalize(cross(N,T) * sign);
    float3x3 TBN = float3x3(T,B,N);
    float3 nTS = txNormal.Sample(samp, uv).xyz * 2 - 1;
    return normalize(mul(nTS, TBN));
}

float4 main(VS_OUT i) : SV_Target
{
    float3 N = normalize(i.NorW);
    if (useNormal==1) N = ApplyNormalMap(N, normalize(i.TanW), i.TanWSign, i.Tex);

    float4 albedo = useDiffuse==1 ? txDiffuse.Sample(samp, i.Tex) : float4(1,1,1,1);
    float alpha   = albedo.a;
    if (useOpacity==1) alpha = txOpacity.Sample(samp, i.Tex).r;

    // Cutout (tree/zelda)
    clip(alpha - (alphaCut));

    // Blinn-Phong
    float3 L = normalize(-vLightDir.xyz);
    float3 V = normalize(EyePosW.xyz - i.WorldPos);
    float3 H = normalize(L + V);

    float diff = max(dot(N, L), 0);
    float ks   = kSAlpha.x;
    float shin = max(kSAlpha.y, 2.0);
    float spec = (diff>0) ? pow(max(dot(N,H),0), shin) * ks : 0;

    // Spec map
    if (useSpecular==1) spec *= txSpecular.Sample(samp, i.Tex).r;

    float3 ambient = I_ambient.rgb * kA.rgb * albedo.rgb;
    float3 lit     = ambient + vLightColor.rgb * (albedo.rgb * diff + spec);

    if (useEmissive==1) lit += txEmissive.Sample(samp, i.Tex).rgb;

    return float4(lit, 1.0);
}
