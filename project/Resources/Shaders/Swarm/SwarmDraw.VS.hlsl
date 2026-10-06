#include "SwarmCommon.hlsli"

StructuredBuffer<DroneData> gDrones : register(t0);

cbuffer CameraTransform : register(b1)
{
    float4x4 gViewProjection;
    float3 gCameraPos;
    float gDroneScale;
};

struct VSInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
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

VSOutput main(VSInput input, uint instanceID : SV_InstanceID)
{
    VSOutput output;
    DroneData drone = gDrones[instanceID];

    // 非表示/死亡判定
    float scale = (drone.hp <= 0.0f || drone.state == 2) ? 0.0f : gDroneScale;

    // 進行方向（Z軸）から基底ベクトルを計算
    float3 fwd = length(drone.velocity) > 0.001f ? normalize(drone.velocity) : float3(0, 0, 1);
    float3 upRef = abs(fwd.y) > 0.99f ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 right = normalize(cross(upRef, fwd));
    float3 up = cross(fwd, right);

    // モデル頂点をスケール＆回転させてワールド座標へ
    float3 localPos = input.position.xyz * scale;
    float3 worldPos = drone.position + (right * localPos.x + up * localPos.y + fwd * localPos.z);

    output.position = mul(float4(worldPos, 1.0f), gViewProjection);
    output.worldPosition = worldPos;
    output.texcoord = input.texcoord;
    output.normal = normalize(right * input.normal.x + up * input.normal.y + fwd * input.normal.z);
    output.state = drone.state;

    // 状態に応じたベースカラー
    if (drone.state == 1)
    {
        // 突撃(Kamikaze): 危険色（赤・オレンジ）
        output.color = float4(1.0f, 0.2f, 0.1f, 1.0f);
    }
    else
    {
        // 巡航(Cruise): メタリックシアン
        output.color = float4(0.2f, 0.7f, 1.0f, 1.0f);
    }

    return output;
}
