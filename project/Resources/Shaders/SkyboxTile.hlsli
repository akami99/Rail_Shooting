struct VertexShaderInput {
    float4 position : POSITION;
};

struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct TransformationMatrix {
    float4x4 WVP;
    float4x4 World;
    float2 uvScale;
    float2 padding;
};
