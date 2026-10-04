#pragma once
#include "BaseScene.h"
#include "MathTypes.h"
#include <memory>
#include <vector>
#include <numbers>

// 必要なクラスをインクルード or 前方宣言
#include "Camera.h"
#include "DebugCamera.h"
#include "Object3d.h"
#include "Sprite.h"

class TitleScene : public BaseScene {
public:
  void Initialize() override;
  void Update() override;
  void Draw() override;
  void Finalize() override;

private:
  // ゲームカメラの更新
  void UpdateGameCamera();

  // ImGui操作の更新
  void UpdateImGui();

private:
  // カメラ(必ずスマートポインタにすること)
  std::unique_ptr<Camera> camera_;
  struct CameraSettings {
    Vector3 translate;
    Vector3 rotate;
  };
  CameraSettings cameraSettings_{{0.0f, 1.5f, 0.0f}, {0.0f, 0.0f, 0.0f}};

  DebugCamera debugCamera_;
  bool useDebugCamera_ = false;

  // ゲームオブジェクト(必ずスマートポインタにすること)
  // 3Dオブジェクト
  std::unique_ptr<Object3d> floorObject_;
  std::unique_ptr<Object3d> backWallObject_;
  std::unique_ptr<Object3d> playerCoverObject_;
  std::unique_ptr<Object3d> targetCoverObject_;
  std::unique_ptr<Object3d> startBoardObject_;
  // スプライト
  std::unique_ptr<Sprite> titleTextSprite_;

  // オブジェクト設定
  Vector3 floorObjectPosition_{0.0f, 0.0f, 15.0f};

  struct BackWallObjectSettings {
    Vector3 position_;
    Vector3 rotation_;
  };
  BackWallObjectSettings backWallObjectSettings_ = {
      {0.0f, 3.0f, 30.0f}, {0.0f, 0.0f, 0.0f}};

  Vector3 playerCoverObjectPosition_{0.0f, 0.5f, 0.5f};
  Vector3 targetCoverObjectPosition_{-4.5f, 0.3f, 24.5f};

  struct StartBoardObjectSettings {
    Vector3 position_;
    Vector3 rotation_;
  };
  StartBoardObjectSettings startBoardObjectSettings_ = {
      {0.0f, 1.5f, 22.0f}, {0.0f, 0.0f, 0.0f}};

  struct TargetBoardInfo {
    std::unique_ptr<Object3d> targetBoardObject_;
    Vector3 position_ = {0.0f, 0.0f, 0.0f};
    Vector3 rotation_ = {0.0f, std::numbers::pi_v<float>, 0.0f};
  };
  std::vector<TargetBoardInfo> targetBoards_;
  Vector3 targetBoardBasePosition_ = {4.5f, 0.8f, 26.0f};

  // 設定など
  int currentBlendMode_ = 1; // NormalBlend

  /// パス定数

  // モデルファイルパスを保持
  // --- タイル画像をタイリングする ---
  const std::string floorModelDirectory_ = "Resources/Assets/Models/TitleScene/titleFloor";
  const std::string floorModelPath_ = "titleFloor.obj";
  const std::string backWallModelDirectory_ = "Resources/Assets/Models/TitleScene/backWall";
  const std::string backWallModelPath_ = "backWall.obj";
  const std::string playerCoverModelDirectory_ = "Resources/Assets/Models/TitleScene/playerCover";
  const std::string playerCoverModelPath_ = "playerCover.obj";
  const std::string targetCoverModelDirectory_ = "Resources/Assets/Models/TitleScene/targetCover";
  const std::string targetCoverModelPath_ = "targetCover.obj";
  // --- ここまで ---
  // --- 専用画像を利用する ---
  const std::string startBoardModelDirectory_ = "Resources/Assets/Models/TitleScene/start";
  const std::string startBoardModelPath_ = "start.obj";
  const std::string targetBoardModelDirectory_ = "Resources/Assets/Models/TitleScene/target";
  const std::string targetBoardModelPath_ = "target.obj";
  // --- ここまで ---
  // テクスチャファイルパスを保持
  const std::string tilePath_ = "tile.png";
  const std::string titleTextPath_ = "title/titleText.png";
};