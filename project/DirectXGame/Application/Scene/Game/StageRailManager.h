#pragma once
#include "LevelLoader.h"
#include "MathTypes.h"
#include "Object3d.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Camera;

/// <summary>
/// ステージのレール移動軌道計算および地形オブジェクト（フロア・エスカレーター・高台）を管理するクラス
/// </summary>
class StageRailManager {
public:
    StageRailManager() = default;
    ~StageRailManager() = default;

    // レールデータの初期化
    void InitializeRail(const LevelData* levelData);

    // 地形オブジェクトの初期化
    void InitializeTerrain(Camera* camera, const Vector3& transitionEdgeColor);

    // 地形オブジェクトの更新・ディゾルブ解除・描画・解放
    void UpdateTerrain(uint32_t viewIndex, Camera* camera);
    void ClearTerrainDissolve(const Vector3& transitionEdgeColor);
    void DrawTerrain();
    void ForEachTerrainObject(const std::function<void(Object3d*)>& func);
    void Finalize();

#ifdef USE_IMGUI
    void UpdateImGui();
#endif

    // レール座標計算
    Vector3 CalculateRailPosition(float progress) const;
    Vector3 GetCameraRailPosition(float progress, float playerEyeHeight) const;

    // 進行状態アクセッサ
    float GetProgress() const { return cameraProgress_; }
    void SetProgress(float progress) { cameraProgress_ = progress; }
    void AdvanceProgress(float delta) { cameraProgress_ += delta; }
    float GetMaxProgress() const { return maxProgress_; }
    float GetCameraSpeed() const { return kCameraSpeed; }

    bool IsMovementPaused() const { return isMovementPaused_; }
    void SetMovementPaused(bool paused) { isMovementPaused_ = paused; }

private:
    struct FloorSettings {
        Vector3 position;
        Vector3 rotation;
        Vector3 scale;
        Vector2 uvScale;
    };
    struct EscalatorSettings {
        Vector3 position;
        Vector3 rotation;
        Vector3 scale;
    };
    struct HillSettings {
        Vector3 position;
        Vector3 rotation;
        Vector3 scale;
    };

    // レールデータ
    std::vector<LevelData::BezierControlPoint> railPoints_;
    float cameraProgress_ = 0.0f;
    float maxProgress_ = 190.0f;
    bool isMovementPaused_ = false;
    static constexpr float kCameraSpeed = 0.35f;

    // 地形オブジェクト
    std::unique_ptr<Object3d> floorObject_;
    std::unique_ptr<Object3d> escalatorObject_;
    std::unique_ptr<Object3d> hillObject_;

    FloorSettings floorSettings_ = { {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {60.0f, 80.0f} };
    EscalatorSettings escalatorSettings_ = { {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} };
    HillSettings hillSettings_ = { {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} };

    static inline const std::string kFloorModelDirectory = "Resources/Assets/Models/ShootingScene/floor";
    static inline const std::string kFloorModel = "floor.obj";
    static inline const std::string kEscalatorModelDirectory = "Resources/Assets/Models/ShootingScene/escalator";
    static inline const std::string kEscalatorModel = "escalator.obj";
    static inline const std::string kHillModelDirectory = "Resources/Assets/Models/ShootingScene/hill";
    static inline const std::string kHillModel = "hill.obj";
    static inline const std::string kTilePath = "tile.png";
};
