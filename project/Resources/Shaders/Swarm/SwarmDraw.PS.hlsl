Texture2D<float4> gTexture : register(t1);
SamplerState gSampler : register(s0);

struct VSOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    float4 color : COLOR0;
    uint state : STATE0;
};

float4 main(VSOutput input) : SV_TARGET
{
    // シンプルな平行光源
    float3 lightDir = normalize(float3(0.5f, 1.0f, -0.5f));
    float NdotL = saturate(dot(input.normal, lightDir));
    float ambient = 0.35f;
    float diffuse = NdotL * 0.65f;

    float4 texColor = gTexture.Sample(gSampler, input.texcoord);
    // もしテクスチャが真っ白ならベースカラー優先
    float4 baseColor = input.color * texColor;

    // 突撃状態(state == 1)の場合はエミッシブ発光（赤く自己発光）
    float3 emissive = (input.state == 1) ? float3(0.8f, 0.1f, 0.0f) : float3(0, 0, 0);

    float3 finalColor = baseColor.rgb * (ambient + diffuse) + emissive;
    return float4(finalColor, baseColor.a);
}
