#pragma once
#include <MathTypes.h>
#include <cstdint>

namespace Swarm {

enum class DroneState : uint32_t {
    Cruise = 0,   // 巡航 (Boids群れ移動)
    Charge = 1,   // 突撃 (プレイヤーへ直線突撃)
    Dead = 2      // 死亡/非表示
};

// GPU/CPU 共通構造体 (48 bytes, 16バイト境界アライメント)
struct DroneData {
    Vector3 position;           // 位置 (12 bytes)
    float hp;                   // 耐久値 (4 bytes)
    Vector3 velocity;           // 速度ベクトル (12 bytes)
    uint32_t state;             // AI状態 (4 bytes)
    Vector3 target;             // 個別目標地点 (12 bytes)
    float stateTimer;           // 状態タイマー (4 bytes)
};

// 定数バッファ (256バイト境界アライメント)
struct SwarmGlobalConstants {
    Vector3 playerPosition;           // プレイヤー座標 (12 bytes)
    float deltaTime;                  // フレーム経過時間 (4 bytes)
    uint32_t droneCount;              // 総ドローン数 (4 bytes)
    float maxSpeed;                   // 最大移動速度 (4 bytes)
    float neighborRadius;             // Boids近傍探索半径 (4 bytes)
    float separationDist;             // 分離距離 (4 bytes)
    Vector3 boundsMin;                // 移動可能境界最小 (12 bytes)
    float separationWeight;           // 分離ウェイト (4 bytes)
    Vector3 boundsMax;                // 移動可能境界最大 (12 bytes)
    float alignmentWeight;            // 整列ウェイト (4 bytes)
    float cohesionWeight;             // 結合ウェイト (4 bytes)
    float targetWeight;               // 目標誘導ウェイト (4 bytes)
    float attackDistance;             // 突撃移行距離 (4 bytes)
    float kamikazeSpeed;              // 特攻時速度 (4 bytes)
    Vector3 cameraPosition;           // カメラ座標 (12 bytes)
    float padding0;                   // パディング (4 bytes)

    // プレイヤー攻撃（銃撃レイ判定）
    Vector3 rayOrigin;                // 射撃レイ原点 (12 bytes)
    float rayRadius;                  // 判定半径 (4 bytes)
    Vector3 rayDirection;             // 射撃レイ方向 (12 bytes)
    float attackDamage;               // ダメージ量 (4 bytes, 0なら攻撃なし)
};

} // namespace Swarm
