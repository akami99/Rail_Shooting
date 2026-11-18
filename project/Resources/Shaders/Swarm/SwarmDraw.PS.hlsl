Texture2D<float4> gTexture : register(t1);
Texture2D<float4> gMaskTexture : register(t2);
SamplerState gSampler : register(s0);

cbuffer CameraTransform : register(b1)
{
    float4x4 gViewProjection;
    float3 gCameraPos;
    float gDroneScale;
    float gCrashDissolveThreshold;
    float gDissolveEdgeRange;
    float2 gCamPadding0;
    float3 gDissolveEdgeColor;
    float gCamPadding1;
};

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
    float edge = 0.0f;
    if (input.state == 3 && gCrashDissolveThreshold > 0.0f)
    {
        float mask = gMaskTexture.Sample(gSampler, input.texcoord).r;
        if (mask <= gCrashDissolveThreshold)
        {
            discard;
        }
        edge = 1.0f - smoothstep(gCrashDissolveThreshold, gCrashDissolveThreshold + gDissolveEdgeRange, mask);
    }

    // シンプルな平行光源
    float3 lightDir = normalize(float3(0.5f, 1.0f, -0.5f));
    float NdotL = saturate(dot(input.normal, lightDir));
    float ambient = 0.35f;
    float diffuse = NdotL * 0.65f;

    float4 texColor = gTexture.Sample(gSampler, input.texcoord);
    // もしテクスチャが真っ白ならベースカラー優先
    float4 baseColor = input.color * texColor;
    if (input.state == 3)
    {
        // 墜落・残骸状態は焦げた暗めのトーン
        baseColor.rgb = texColor.rgb * float3(0.28f, 0.30f, 0.35f);
    }

    // 突撃状態(state == 1)の場合はエミッシブ発光（赤く自己発光）
    float3 emissive = (input.state == 1) ? float3(0.8f, 0.1f, 0.0f) : float3(0, 0, 0);

    float3 finalColor = baseColor.rgb * (ambient + diffuse) + emissive + edge * gDissolveEdgeColor;
    return float4(finalColor, baseColor.a);
}
