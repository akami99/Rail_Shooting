#pragma once

#include "SwarmTypes.h"
#include "Model/Model.h"
#include "Camera/Camera.h"
#include "Sprite/Sprite.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <vector>
#include <string>

class SwarmManager {
public:
    // カメラ変換用定数バッファ
    struct CameraTransformBuffer {
        Matrix4x4 viewProjection;
        Vector3 cameraPosition;
        float droneScale = 1.5f;
        float crashDissolveThreshold = 0.0f;
        float dissolveEdgeRange = 0.08f;
        Vector2 padding0 = { 0.0f, 0.0f };
        Vector3 dissolveEdgeColor = { 0.0f, 1.0f, 0.8f };
        float padding1 = 0.0f;
    };

    struct AttackCommand {
        Vector3 rayOrigin = { 0.0f, 0.0f, 0.0f };
        Vector3 rayDirection = { 0.0f, 0.0f, 1.0f };
        float rayRadius = 2.0f;
        float damage = 0.0f;
    };

    // 突撃グループ構造体（第二形態・最終形態用）
    struct ChargeGroup {
        std::vector<uint32_t> droneIndices;
        std::vector<Vector3> localOffsets;
        Vector3 centerPos = { 0.0f, 0.0f, 0.0f };
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };
        float speed = 6.0f;
        float hitRadius = 2.2f;
        int hp = 1;
        int maxHp = 1;
        float damageBarRatio = 1.0f;
        bool isFinalGroup = false;
        bool isDead = false;
        bool isHpGaugeVisible = false;
        std::unique_ptr<Sprite> hpBgSprite;
        std::unique_ptr<Sprite> hpDamageSprite;
        std::unique_ptr<Sprite> hpFillSprite;
        std::unique_ptr<Sprite> hpFrameSprite;
    };

    // フレームごとのイベント通知（エフェクト・スコア・被弾・クリア判定用）
    struct FrameEvents {
        std::vector<Vector3> hitPositions;
        std::vector<Vector3> destroyedPositions;
        std::vector<Vector3> selfDestructPositions;
        int dronesKilledByPlayer = 0;
        int playerDirectHits = 0;     // 直撃ヒット数（通常時：10ダメージ）
        int playerGroupBlastHits = 0; // グループ自爆ヒット数（カバー時：3ダメージ）
        bool bossDefeated = false;
    };

public:
    SwarmManager() = default;
    ~SwarmManager() = default;

    // 初期化
    void Initialize(uint32_t droneCount = 512,
                    const std::string& modelDirectory = "Resources/Assets/Models/ShootingScene/enemy",
                    const std::string& modelFilename = "enemy.obj");

    // シミュレーション更新
    void Update(float deltaTime, const Vector3 & playerPos, Camera* camera, const AttackCommand* attackCmd = nullptr, bool isPlayerCovering = false);

    // 描画 (GPU Instancing)
    void Draw(Camera* camera);

    // 突撃グループの頭上HPゲージ更新・描画
    void UpdateHpGauges(Camera* camera, float clientWidth, float clientHeight);
    void DrawHpGauges();

    // ドローンのスポーン（指定位置を中心に半径radius以内にcount機を配置：主にイントロ演出用）
    void SpawnDrones(uint32_t count, const Vector3 & centerPos, float radius = 20.0f);

    // ボス戦の開始（正面少し上に集め、3区間移動・攻撃ロジックを有効化）
    void StartBossBattle(uint32_t count, const Vector3& playerBasePos, float baseYaw, bool keepExistingPositions = false);

    // ボス登場侵入演出の開始と更新
    void StartBossEntrance(uint32_t count, const Vector3& playerBasePos, float baseYaw, const Vector3& spawnPos);
    void UpdateBossEntrance(float deltaTime, const Vector3& anchorPos, Camera* camera);

    // 指定機数の突撃を発動
    void TriggerCharge(uint32_t count, const Vector3& playerPos);

    // ドローンをクリア
    void ClearDrones() {
        isActive_ = false;
        pendingSpawn_ = false;
        isBossMode_ = false;
        crashDissolveThreshold_ = 0.0f;
        chargeGroups_.clear();
        std::fill(crashSleeping_.begin(), crashSleeping_.end(), false);
    }

    // スウォームがアクティブ（スポーン済み）か
    bool IsActive() const { return isActive_; }
    bool IsBossMode() const { return isBossMode_; }
    bool IsBossDefeated() const { return isBossDefeated_; }

    // ボス戦カメラ区間 (-1: 左, 0: 正面, +1: 右) と目標カメラ角度
    int GetCurrentCameraSection() const { return currentSection_; }
    float GetTargetCameraYaw() const;
    float GetTargetCameraPitch() const;

    // 照準レイがドローンまたは突撃グループに当たっているか
    bool IsRayHitting(const Vector3& rayOrigin, const Vector3& rayDir) const;

    // 自動照準用のターゲット座標を取得 (優先順位: 突撃グループ -> 個別突撃機 -> 巡航機)
    bool GetAutoAimTarget(Vector3& outTargetPos) const;

    // 墜落ドローン一斉ディゾルブ閾値設定 (0.0: 通常, 0.0〜1.0: 消滅進行)
    void SetCrashDissolveThreshold(float threshold) { crashDissolveThreshold_ = threshold; }

    // 墜落残骸ドローンの中心座標を取得
    bool GetCrashCenter(Vector3& outCenter) const;

    // 直近フレームのイベント取得
    const FrameEvents& GetFrameEvents() const { return frameEvents_; }

    // ボス形態 (1: 第一形態, 2: 第二形態, 3: 最終形態)
    int GetBossPhase() const;
    uint32_t GetMaxBossHp() const { return bossMaxHp_; }
    float GetNoShootTimer() const { return noShootTimer_; }
    float GetPeriodicAttackTimer() const { return periodicAttackTimer_; }
    float GetEvadeCooldownTimer() const { return evadeCooldownTimer_; }

    // リスポーン設定 (最大リスポーン数制限)
    void SetMaxRespawnCount(uint32_t count) { maxRespawnCount_ = count; }
    uint32_t GetMaxRespawnCount() const { return maxRespawnCount_; }
    uint32_t GetTotalRespawnedCount() const { return totalRespawnedCount_; }

    // 突撃以降距離の設定(0.0f にすると突撃しない)
    void SetAttackDistance(float dist) { settings_.attackDistance = dist; }

    // パラメータ取得 (ImGui等での調整用)
    Swarm::SwarmGlobalConstants& GetSettings() { return settings_; }
    uint32_t GetDroneCount() const { return droneCount_; }
    float& GetDroneScale() { return droneScale_; }
    float GetDroneScale() const { return droneScale_; }
    void SetDroneScale(float scale) { droneScale_ = scale; }
    float& GetCrashCollisionRadiusScale() { return crashCollisionRadiusScale_; }
    float GetCrashCollisionRadiusScale() const { return crashCollisionRadiusScale_; }
    float& GetCrashGroundFriction() { return crashGroundFriction_; }
    float& GetCrashBounciness() { return crashBounciness_; }
    float& GetCrashPushStrength() { return crashPushStrength_; }
    const std::vector<Swarm::DroneData>& GetDronesData() const { return dronesShadow_; }
    uint32_t GetAliveDroneCount() const;

private:
    void InitializeBuffers();
    void InitializePipeline();
    void InitializeModel(const std::string& directory, const std::string& filename);
    void CreateChargeGroup(const std::vector<uint32_t>& indices, bool isFinalGroup, const Vector3& playerPos);
    void UpdateBossSimulation(float deltaTime, const Vector3& playerPos, const AttackCommand* attackCmd, bool isPlayerCovering);
    void UpdateIntroSimulation(float deltaTime, const Vector3& targetCenter);
    void UpdateCrashingDrones(float deltaTime);

private:
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    // ドローン総数
    uint32_t droneCount_ = 512;

    // Ping-Pong StructuredBuffer (Buffer 0 & Buffer 1)
    ComPtr<ID3D12Resource> droneBuffers_[2];
    ComPtr<ID3D12Resource> intermediateUploadBuffer_;
    uint32_t srvIndices_[2] = { 0, 0 };
    uint32_t uavIndices_[2] = { 0, 0 };
    uint32_t readIndex_ = 0;
    uint32_t writeIndex_ = 1;

    // 定数バッファ
    ComPtr<ID3D12Resource> constantBuffer_;
    Swarm::SwarmGlobalConstants* mappedConstants_ = nullptr;

    ComPtr<ID3D12Resource> cameraConstantBuffer_;
    CameraTransformBuffer* mappedCameraConstants_ = nullptr;

    // パイプライン (Compute)
    ComPtr<ID3D12RootSignature> computeRootSignature_;
    ComPtr<ID3D12PipelineState> computePSO_;

    // パイプライン (Draw)
    ComPtr<ID3D12RootSignature> drawRootSignature_;
    ComPtr<ID3D12PipelineState> drawPSO_;

    // 描画モデル
    std::unique_ptr<Model> droneModel_;
    uint32_t textureSrvIndex_ = 0;
    uint32_t dissolveMaskSrvIndex_ = 0;
    float crashDissolveThreshold_ = 0.0f;
    float bossGroundY_ = 0.0f;
    float crashCollisionRadiusScale_ = 0.5f;
    float crashGroundFriction_ = 0.82f;
    float crashBounciness_ = 0.25f;
    float crashPushStrength_ = 0.4f;

    // シミュレーション設定値
    Swarm::SwarmGlobalConstants settings_{};

    // リスポーン制限
    uint32_t maxRespawnCount_ = 200;
    uint32_t totalRespawnedCount_ = 0;
    float respawnTimer_ = 0.0f;

    // スポーン状態
    bool isActive_ = false;
    bool pendingSpawn_ = false;
    bool needGpuSync_ = false;
    std::vector<Swarm::DroneData> pendingSpawnData_;
    std::vector<Swarm::DroneData> dronesShadow_;
    std::vector<bool> crashSleeping_;
    float droneScale_ = 0.2f;

    // ボス戦専用パラメータ
    bool isBossMode_ = false;
    bool isBossDefeated_ = false;
    bool isFinalPhaseTriggered_ = false;
    uint32_t bossMaxHp_ = 100;
    Vector3 bossPlayerBasePos_ = { 0.0f, 0.0f, 0.0f };
    float bossBaseYaw_ = 0.0f;
    float bossMoveTimer_ = 0.0f;
    float noShootTimer_ = 0.0f;
    float periodicAttackTimer_ = 0.0f;
    float evadeCooldownTimer_ = 0.0f;
    int currentSection_ = 0; // -1: 左, 0: 正面, +1: 右

    std::vector<ChargeGroup> chargeGroups_;
    FrameEvents frameEvents_{};
};
