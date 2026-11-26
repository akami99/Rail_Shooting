#pragma once
#include "MathTypes.h"
#include <cstdint>
#include <string>

/// <summary>
/// シューティングシーンのパーティクルエフェクトの初期化・発生・完了判定を管理するクラス
/// </summary>
class ShootingEffectManager {
public:
    ShootingEffectManager() = default;
    ~ShootingEffectManager() = default;

    // 初期化（全パーティクルグループ・エミッター生成）
    void Initialize();

    // 各種エフェクト発生
    void EmitPlayerHit(const Vector3& hitPos);
    void EmitBossCharge(const Vector3& pos);
    void EmitBossAttack(const Vector3& pos);
    void EmitReloadComplete(const Vector3& pos);
    void EmitAmmoSpark(const Vector3& pos, uint32_t count);
    void EmitSpark(const Vector3& pos, uint32_t count = 24);
    void EmitCylinder(const Vector3& pos, uint32_t count = 8);

    // クリア待機対象エフェクトの再生状態確認・停止
    bool AreClearEffectsFinished() const;
    void StopClearWaitEffects();

    // 全パーティクルクリア
    void ClearAll();

private:
    void InitializeSceneEffects();
};
