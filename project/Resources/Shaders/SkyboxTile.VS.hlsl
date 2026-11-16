#include "SkyboxTile.hlsli"

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VertexShaderOutput main(VertexShaderInput input, uint vertexId : SV_VertexID) {
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP).xyww;

    // calculate 2D UV (0..1)
    // 0: (0, 0), 1: (1, 0), 2: (0, 1), 3: (1, 1)
    float2 baseUV = float2(
        (vertexId % 2 == 1) ? 1.0f : 0.0f,
        ((vertexId % 4) >= 2) ? 1.0f : 0.0f
    );
    output.texcoord = baseUV * gTransformationMatrix.uvScale;
    return output;
}
