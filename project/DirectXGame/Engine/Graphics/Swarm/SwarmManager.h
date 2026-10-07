#pragma once

#include "SwarmTypes.h"
#include "Model/Model.h"
#include "Camera/Camera.h"
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
    };

    struct AttackCommand {
        Vector3 rayOrigin = { 0.0f, 0.0f, 0.0f };
        Vector3 rayDirection = { 0.0f, 0.0f, 1.0f };
        float rayRadius = 2.0f;
        float damage = 0.0f;
    };

public:
    SwarmManager() = default;
    ~SwarmManager() = default;

    // 初期化
    void Initialize(uint32_t droneCount = 512,
                    const std::string& modelDirectory = "Resources/Assets/Models/ShootingScene/enemy",
                    const std::string& modelFilename = "enemy.obj");

    // シミュレーション更新 (Compute Shader)
    void Update(float deltaTime, const Vector3 & playerPos, Camera* camera, const AttackCommand* attackCmd = nullptr);

    // 描画 (GPU Instancing)
    void Draw(Camera* camera);

    // ドローンのスポーン（指定位置を中心に半径radius以内にcount機を配置）
    void SpawnDrones(uint32_t count, const Vector3 & centerPos, float radius = 20.0f);

    // スウォームがアクティブ（スポーン済み）か
    bool IsActive() const { return isActive_; }

    // リスポーン設定 (最大リスポーン数制限)
    void SetMaxRespawnCount(uint32_t count) { maxRespawnCount_ = count; }
    uint32_t GetMaxRespawnCount() const { return maxRespawnCount_; }
    uint32_t GetTotalRespawnedCount() const { return totalRespawnedCount_; }

    // パラメータ取得 (ImGui等での調整用)
    Swarm::SwarmGlobalConstants& GetSettings() { return settings_; }
    uint32_t GetDroneCount() const { return droneCount_; }
    float& GetDroneScale() { return droneScale_; }
    float GetDroneScale() const { return droneScale_; }
    void SetDroneScale(float scale) { droneScale_ = scale; }
    const std::vector<Swarm::DroneData>& GetDronesData() const { return dronesShadow_; }
    uint32_t GetAliveDroneCount() const;

private:
    void InitializeBuffers();
    void InitializePipeline();
    void InitializeModel(const std::string& directory, const std::string& filename);

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

    // シミュレーション設定値
    Swarm::SwarmGlobalConstants settings_{};

    // リスポーン制限
    uint32_t maxRespawnCount_ = 200;
    uint32_t totalRespawnedCount_ = 0;
    float respawnTimer_ = 0.0f;

    // スポーン状態
    bool isActive_ = false;
    bool pendingSpawn_ = false;
    std::vector<Swarm::DroneData> pendingSpawnData_;
    std::vector<Swarm::DroneData> dronesShadow_;
    float droneScale_ = 0.2f;
};
