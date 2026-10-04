#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s1); // CLAMPサンプラーを使用

cbuffer GaussianBlurParams : register(b0)
{
    int gKernelSize;  // カーネルサイズ (3, 5, 7, 9, ...)
    float gSigma;     // 標準偏差 (シグマ)
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

static const float PI = 3.1415926535f;

float gauss(float x, float y, float sigma)
{
    float s = max(sigma, 0.0001f); // ゼロ除算防止
    float exponent = -(x * x + y * y) * rcp(2.0f * s * s);
    float denominator = 2.0f * PI * s * s;
    return exp(exponent) * rcp(denominator);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 uvStepSize = float2(1.0f / float(width), 1.0f / float(height));
    
    int radius = (gKernelSize - 1) / 2;
    radius = clamp(radius, 0, 15); // ループ数制限用のガード
    
    float3 sumColor = float3(0.0f, 0.0f, 0.0f);
    float totalWeight = 0.0f;
    
    for (int x = -radius; x <= radius; ++x)
    {
        for (int y = -radius; y <= radius; ++y)
        {
            float2 offset = float2(x, y) * uvStepSize;
            float3 fetchColor = gTexture.Sample(gSampler, input.texcoord + offset).rgb;
            
            float weight = gauss(float(x), float(y), gSigma);
            sumColor += fetchColor * weight;
            totalWeight += weight;
        }
    }
    
    // 正規化処理
    if (totalWeight > 0.0f)
    {
        output.color.rgb = sumColor * rcp(totalWeight);
    }
    else
    {
        output.color.rgb = gTexture.Sample(gSampler, input.texcoord).rgb;
    }
    output.color.a = 1.0f;
    
    return output;
}
