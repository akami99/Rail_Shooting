#pragma once
#include "EnemyProjectile.h"
#include "LevelLoader.h"
#include "MathTypes.h"
#include "Model.h"
#include "Object3d.h"
#include "ShootingDemoAI.h"
#include "Sprite.h"
#include "Swarm/SwarmManager.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Camera;
class ShootingEffectManager;
class StageRailManager;

/// <summary>
/// Observer パターン：ウェーブ・敵・スウォーム戦イベントの購読インターフェース
/// </summary>
class IShootingEventObserver {
public:
    virtual ~IShootingEventObserver() = default;
    virtual void OnScoreEarned(int scoreDelta) { (void)scoreDelta; }
    virtual void OnEliteEnemyDefeated(const Vector3& worldPos) { (void)worldPos; }
};

/// <summary>
/// シューティングシーンの通常敵ウェーブ・プロジェクタイル・スウォーム/ボス戦を管理するクラス
/// </summary>
class ShootingWaveManager {
public:
    struct PlayerHitReport {
        int coverBlastHits = 0;
        int directNormalHits = 0;
        int directBlastHits = 0;
        int jammingHits = 0;
        bool triggerDirectBlastFx = false;
        bool triggerDirectNormalFx = false;
    };

    struct SwarmStepResult {
        int groupBlastHits = 0;
        int directHits = 0;
        bool bossDefeated = false;
    };

    static constexpr uint32_t kMaxSwarmCount = 128;

    ShootingWaveManager() = default;
    ~ShootingWaveManager() = default;

    void Initialize(const LevelData* levelData,
                    const StageRailManager& railManager,
                    Camera* camera,
                    const std::string& enemyModelDir,
                    const std::string& enemyModelName,
                    const Vector3& introSwarmOffset,
                    float introSwarmRadius,
                    uint32_t introSwarmCount);
    void Finalize();

    // イントロ・ボス登場・終了フェーズ制御
    void UpdateIntroSwarm(float deltaTime, const Vector3& swarmCenter, Camera* camera);
    void ResetSwarmToBossSpawn(const Vector3& basePos, float baseYaw);
    void ClearSwarmDrones();
    void ClearAmbientSwarmDrones();
    void ClearProjectiles();
    void UpdateAliveEnemiesTransforms(int mainViewIndex, Camera* camera);

    void StartBossIntro(const Vector3& basePos, float baseYaw);
    void UpdateBossIntro(float phaseTimer, float bossIntroTotalDuration, const Vector3& basePos, float baseYaw, Camera* camera);
    void BeginSwarmBattle(const Vector3& basePos, float baseYaw);
    void UpdateAmbientSwarm(float deltaTime, const Vector3& anchor, Camera* camera);
    void UpdateBossDefeatLookDown(float phaseTimer, float deltaTime, const Vector3& basePos, float baseYaw,
                                  Camera* camera, float& ioCameraYaw, float& ioCameraPitch);

    // プレイ中・ボス戦メイン更新
    void UpdateRailEnemyTriggers(StageRailManager& railManager);
    void UpdateEnemiesAndProjectiles(float deltaTime, float railProgress, const Vector3& playerPos, int mainViewIndex, Camera* camera);
    PlayerHitReport ResolveProjectileHits(bool isCovering, bool isInvincible, bool isForceInvincible,
                                          const Vector3& railTargetPos, const Vector3& camPos,
                                          int playerHp, int directDamage);
    void RemoveDeadProjectiles();
    void ProcessPlayerShot(const Vector2& mousePos, float clientWidth, float clientHeight,
                           Camera* camera, float railProgress, ShootingEffectManager* effectManager);
    bool UpdateAutoClearBossAttack(float deltaTime, Camera* camera, float& autoClearAttackTimer, Vector3& outTargetPos);

    // Observer 管理
    void AddObserver(IShootingEventObserver* observer);
    void RemoveObserver(IShootingEventObserver* observer);
    SwarmStepResult UpdateSwarmSimulation(float deltaTime, const Vector3& playerPos, Camera* camera,
                                          bool isCovering, ShootingEffectManager* effectManager);
    void UpdateHpGauges(Camera* camera, float clientWidth, float clientHeight, float railProgress, bool isSwarmBattle);

    // 判定・情報取得
    bool CheckAiming(const Vector3& nearPos, const Vector3& rayDir, float railProgress) const;
    bool AreAllEnemiesDead() const;
    float GetNextBattleDistance(float currentProgress, float maxProgress) const;
    void RespawnIntroSwarm(uint32_t count, const Vector3& center, float radius);
    std::vector<bool> ComputeSectionCleared(const std::vector<float>& sectionProgresses) const;
    void PopulateDemoAIContext(ShootingDemoAI::Context& ctx, float railProgress, const Vector3& camPos, bool isSwarmBattle) const;

    bool IsBossDefeated() const;
    bool IsBossMode() const;
    bool IsSwarmActive() const;
    uint32_t GetAliveDroneCount() const;
    bool GetBossCameraTarget(float& outYaw, float& outPitch) const;
    float GetSwarmBattleTimer() const { return swarmBattleTimer_; }
    void AddSwarmBattleTimer(float dt) { swarmBattleTimer_ += dt; }

    // リセット・ジャンプ
    void ResetForJumpToSection(float targetProgress, int mainViewIndex, Camera* camera, int& outClearedScore);
    void ResetForGameReset(int mainViewIndex, Camera* camera);
    void JumpToBossBattle(const Vector3& basePos, float baseYaw);
    void KillAllForAutoClear();

    // 描画
    void DrawEnemiesAndProjectiles(float railProgress, bool drawEnemies, Camera* camera);
    void DrawHpGauges(bool isSwarmBattle);

#ifdef USE_IMGUI
    void UpdateObject3dImGui(Camera* camera, bool& outRequestedSwarmBattle);
    bool UpdateSwarmCollisionDebugImGui(float& outDroneScale, float& outColRadius);
#endif // USE_IMGUI

private:
    enum class EnemyType {
        Normal,
        Engineer,
        Elite
    };

    /// <summary>
    /// 敵種別ごとの振る舞いを抽象化する基底クラス
    /// </summary>
    class IEnemyBehavior {
    public:
        virtual ~IEnemyBehavior() = default;
        virtual EnemyType GetType() const = 0;
        virtual int GetMaxHp() const = 0;
        virtual Vector4 GetModelColor() const = 0;
        virtual float GetSpawnInterval(float normalInterval, float engineerInterval) const = 0;
        virtual float GetHitShotRate(float baseRate) const = 0;
        virtual Vector4 GetChargeRingColor(int hitShotCount) const = 0;
        virtual EnemyProjectile::Type SelectHitProjectileType(int hitShotCount) const = 0;
        virtual EnemyProjectile::Type SelectMissProjectileType() const = 0;
        virtual bool DropsMachineGunAmmoOnDeath() const { return false; }
        virtual void PrepareFirstShotIfNeeded(float chargeStart, float& shootTimer,
                                              bool& isNextHitShot, bool hasFiredFirstShot) const {
            (void)chargeStart; (void)shootTimer; (void)isNextHitShot; (void)hasFiredFirstShot;
        }
        virtual bool ShouldMarkFirstShotFired() const { return false; }
    };

    struct EnemyInfo {
        std::unique_ptr<Object3d> object;
        std::unique_ptr<Model> model;
        std::unique_ptr<IEnemyBehavior> behavior;
        Vector3 basePosition;
        Vector3 baseRotation;
        float distance = 0.0f;
        bool isActive = false;
        bool isDead = false;
        float shootTimer = 0.0f;
        float spawnTimer = 0.0f;
        int shootCount = 0;
        int hitShotCount = 0;
        bool isNextHitShot = false;
        bool isHitShotReserved = false;
        bool hasFiredEliteFirstShot = false;
        Vector3 initialOffsetPosition = { 0.0f, 0.0f, 0.0f };
        float slideTimer = 0.0f;
        float slideDelay = 0.0f;
        float slideDuration = 1.0f;
        float deathTimer = 0.0f;
        int hp = 1;
        int maxHp = 1;
        float damageBarRatio = 1.0f;
        bool isHpGaugeVisible = false;
        std::unique_ptr<Sprite> hpBgSprite;
        std::unique_ptr<Sprite> hpDamageSprite;
        std::unique_ptr<Sprite> hpFillSprite;
        std::unique_ptr<Sprite> hpFrameSprite;
    };

    static std::unique_ptr<IEnemyBehavior> CreateEnemyBehavior(EnemyType type);
    void ApplyEnemyType(EnemyInfo& enemy, EnemyType type, bool resetHpAndGauge = true) const;

    class NormalEnemyBehavior;
    class EngineerEnemyBehavior;
    class EliteEnemyBehavior;

    Vector3 CalculateRingRotation(const Vector3& direction) const;
    Vector3 CalculateBossEntranceAnchor(float t, const Vector3& basePos, float baseYaw) const;
    void NotifyScoreEarned(int scoreDelta);
    void NotifyEliteEnemyDefeated(const Vector3& worldPos);

    // Object Pool パターン：弾種ごとの再利用プール
    std::unique_ptr<EnemyProjectile> AcquireProjectile(EnemyProjectile::Type type,
                                                       const Vector3& position,
                                                       const Vector3& velocity);
    void ReleaseProjectileToPool(std::unique_ptr<EnemyProjectile> projectile);

    std::vector<IShootingEventObserver*> observers_;
    std::array<std::vector<std::unique_ptr<EnemyProjectile>>, 3> projectilePools_;

    static constexpr uint32_t kSpawnSwarmCount = 100;
    static constexpr uint32_t kAmbientSwarmCount = 1000;
    static constexpr float kProjectileSpawnInterval = 120.0f;
    static constexpr float kEngineerSpawnInterval = 200.0f;
    static constexpr float kChargeDuration = 36.0f;
    static constexpr int kMaxSquadHitShots = 2;
    static constexpr float kSquadHitShotCooldownDuration = 2.0f;

    std::vector<EnemyInfo> enemies_;
    std::vector<std::unique_ptr<EnemyProjectile>> projectiles_;

    float speed_ = 1.2f;
    float engineerProjectileSpeed_ = 0.18f;
    float jammingProjectileSpeed_ = 1.8f;

    float hitShotRate_ = 0.35f;
    float missShotSpread_ = 3.0f;
    int squadHitShotCount_ = 0;
    int squadFiredHitShotCount_ = 0;
    float squadHitShotCooldown_ = 0.0f;

    // スウォーム・ボス戦管理
    std::unique_ptr<SwarmManager> swarmManager_;
    std::unique_ptr<SwarmManager> ambientSwarmManager_;
    SwarmManager::AttackCommand pendingSwarmAttack_{};
    bool hasPendingSwarmAttack_ = false;
    float swarmBattleTimer_ = 0.0f;
    bool hasSpawnedBossEntrance_ = false;

    // ボス全体HPゲージUI
    std::unique_ptr<Sprite> bossHpBgSprite_;
    std::unique_ptr<Sprite> bossHpDamageSprite_;
    std::unique_ptr<Sprite> bossHpFillSprite_;
    std::unique_ptr<Sprite> bossHpFrameSprite_;
    float bossDamageBarRatio_ = 1.0f;
};
