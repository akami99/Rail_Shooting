#pragma once
#include "BaseScene.h"
#include "MathTypes.h"
#include <memory>
#include <vector>
#include <numbers>

#include "Camera.h"
#include "DebugCamera.h"
#include "Object3d.h"
#include "Sprite.h"
#include "EnemyProjectile.h"

class TitleScene : public BaseScene {
public:
  void Initialize() override;
  void Update() override;
  void Draw() override;
  void Finalize() override;

private:
  // 照準更新
  void UpdateCrosshairAndRaycast();
  // カバー更新
  void UpdateCover();
  // プレイヤーの射撃処理
  void UpdatePlayerShot();
  // 的の倒れ・起き上がり・タイマー更新
  void UpdateTargetsAnimation();
  // 看板の倒れ更新＆シーン遷移
  void UpdateStartBoardAnimation();
  // 敵の交互射撃ルーチン
  void UpdateEnemyShooting();
  // 弾の移動・自動カバー判定・消滅処理
  void UpdateProjectiles();

  // ゲームカメラの更新
  void UpdateGameCamera();

  // ImGui操作の更新
  void UpdateImGui();

  // 直方体とレイの交差判定
  bool IntersectRayAABB(const Vector3& rayOrigin, const Vector3& rayDir, const AABB& aabb, float& outT);

  bool IsTargetHiddenByCover(int targetIdx);

  // 中心座標とサイズから AABB を生成するヘルパー
  AABB MakeAABB(const Vector3 &center, const Vector3 &size);

private:
  // カメラ(必ずスマートポインタにすること)
  std::unique_ptr<Camera> camera_;
  struct CameraSettings {
    Vector3 translate;
    Vector3 rotate;
    float fovY;
  };
  CameraSettings cameraSettings_{{0.0f, 1.5f, -1.5f}, {0.0f, 0.0f, 0.0f}, 45.0f};

  DebugCamera debugCamera_;
  bool useDebugCamera_ = false;

  // --- 的の状態 ---
  enum class TargetState {
      Standing, // 直立（撃てる状態）
      Falling,  // 倒れ中（アニメーション）
      Down,     // 倒れ切って待機中（5秒タイマー中）
      Rising    // 起き上がり中（アニメーション）
  };

  // ゲームオブジェクト(必ずスマートポインタにすること)
  // 3Dオブジェクト
  std::unique_ptr<Object3d> floorObject_;
  std::unique_ptr<Object3d> backWallObject_;
  std::unique_ptr<Object3d> playerCoverObject_;
  std::unique_ptr<Object3d> startBoardObject_;
  // スプライト
  std::unique_ptr<Sprite> titleTextSprite_;
  std::unique_ptr<Sprite> controlSprite_;
  std::unique_ptr<Sprite> crosshairSprite_;

  // オブジェクト設定
  struct FloorObjectSettings {
      Vector3 position_;
      Vector3 scale_;
      Vector2 uvScale_;
  };
  FloorObjectSettings floorObjectSettings_ = {
      {0.0f, 0.0f, -15.0f}, {4.0f, 1.0f, 3.0f}, { 10.0f, 10.0f } };

  struct BackWallObjectSettings {
      Vector3 position_;
      Vector3 scale_;
      Vector2 uvScale_;
  };
  BackWallObjectSettings backWallObjectSettings_ = {
      {0.0f, 9.0f, 30.0f}, {4.0f, 3.0f, 1.0f}, { 10.0f, 4.0f } };

  struct PlayerCoverObjectSettings {
      Vector3 position_;
      Vector2 uvScale_;
  };
  PlayerCoverObjectSettings playerCoverObjectSettings_ = {
      {0.0f, 0.5f, 0.5f}, {1.0f, 8.0f}};

  struct TargetCoverInfo {
      std::unique_ptr<Object3d> object_;
      Vector3 position_ = { 0.0f, 0.0f, 0.0f };
      Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
      Vector3 aabbSize_ = { 1.8f, 1.0f, 0.5f };
      Vector2 uvScale_ = { 1.0f, 8.0f };
  };
  std::vector<TargetCoverInfo> targetCovers_;

  // 4つのカバー設定(左から順に: 縦 → 横 → 縦(左半分隠し) → 横)
  struct CoverConfig {
      Vector3 pos;
      bool isVertical;
  };
  CoverConfig coverConfigs_[4] = {
      { {-7.5f, 0.9f, 24.5f}, true  }, // 1. 左外（縦長・動き始め）
      { {-4.0f, 0.3f, 24.5f}, false }, // 2. 左内（横長・最初のカバー）
      { { 4.5f, 0.9f, 24.5f}, true  }, // 3. 右内（縦長・敵の半分を隠すように配置）
      { { 7.5f, 0.3f, 24.5f}, false }  // 4. 右外（横長・動き始め）
  };
  const Vector3 kCoverVerticalSize = { 1.0f, 1.8f, 0.5f };; // 縦長カバーのサイズ
  const Vector3 kCoverVerticalRotate = { 0.0f, 0.0f, std::numbers::pi_v<float> *0.5f };; // 縦長カバーの回転角度

  struct StartBoardObjectInfo {
    Vector3 position_;
    Vector3 rotation_;
    Vector3 aabbSize_;
    TargetState state_ = TargetState::Standing;
    float fallAngle_ = 0.0f; // 倒れた角度
  };
  StartBoardObjectInfo startBoardObjectSettings_ = {
      {0.0f, 0.0f, 22.0f}, {0.0f, 0.0f, 0.0f}, {3.0f, 1.5f, 0.3f}};

  struct TargetBoardInfo {
    std::unique_ptr<Object3d> targetBoardObject_;
    Vector3 basePosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 rotation_ = {0.0f, 0.0f, 0.0f};
    Vector3 aabbSize_ = {1.0f, 1.5f, 0.3f};
    TargetState state_ = TargetState::Standing;
    float fallAngle_ = 0.0f; // 倒れた角度
    float downTimer_ = 0.0f; // 倒れてからの経過時間(カウント用)
    bool isMoving_ = false;      // 移動するか
    float moveSpeed_ = 0.0f;     // 移動速度（角周波数）
    float moveAmplitude_ = 1.0f; // 振幅（±1.0m で計2m移動）
    float moveTimer_ = 0.0f;     // 移動用タイマー
  };
  std::vector<TargetBoardInfo> targetBoards_;

  // 4体の初期配置(左外、左内、右内、右外)
  struct SpawnConfig {
      Vector3 pos;
      bool isMoving;
      float speed;
  };
  SpawnConfig configs_[4] = {
      { {-7.5f, 0.0f, 26.0f}, true,  1.5f }, // 0: 左外 (移動・少しゆっくり)
      { {-4.0f, 0.0f, 26.0f}, false, 0.0f }, // 1: 左内 (固定・カバー裏)
      { { 4.0f, 0.0f, 26.0f}, false, 0.0f }, // 2: 右内 (固定・カバー裏)
      { { 7.5f, 0.0f, 26.0f}, true,  2.3f }  // 3: 右外 (移動・速め)
  };

  // スプライト設定
  struct TitleTextSpriteSettings {
    Vector2 translate_;
    Vector2 scale_;
  };
  TitleTextSpriteSettings titleTextSpriteSettings_ = {
      {640.0f, 180.0f}, {320.0f, 256.0f} };

  struct ControlSpriteSettings {
    Vector2 translate_;
    Vector2 scale_;
  };
  ControlSpriteSettings controlSpriteSettings_ = {
      {1120.0f, 620.0f}, {256.0f, 128.0f} };

  struct CrosshairSpriteSettings {
    Vector4 colorAim_;   // 攻撃可能時の色
    Vector4 colorNoAim_; // 攻撃不可能時の色
  };
  CrosshairSpriteSettings crosshairSpriteSettings_ = {
      {1.0f, 0.0f, 0.0f, 1.0f},  // 攻撃可能時は赤
      {0.0f, 1.0f, 0.0f, 0.4f}}; // 攻撃不可能時は緑

  // 設定など
  int currentBlendMode_ = 1; // NormalBlend

  // --- カバー管理 ---
  bool isCovering_ = false;         // カバー中かどうか
  float coverYOffset_ = 0.0f;       // カバー時のY座標オフセット
  const float coverYTarget_ = -0.5f; // カバー時のY座標目標値

  // --- 照準中のターゲット情報 ---
  enum class TargetType { None, Cover, StartBoard, TargetBoard };
  TargetType currentAimTarget_ = TargetType::None;
  int aimedTargetIndex_ = -1; // 狙っている的の番号 (0〜3)

  // --- 敵の射撃管理 ---
  std::vector<std::unique_ptr<EnemyProjectile>> projectiles_;
  const Vector3 kShootOffset = {-0.3f, 1.0f, -0.5f}; // 射撃位置のオフセット
  float shootTimer_ = 0.0f;
  const float kShootInterval_ = 3.0f; // 発射間隔（3秒）
  int nextShootTargetIndex_ = 0;       //次に撃つ的（0: 左, 1: 右）
  const float kEnemyBulletSpeed_ = 0.8f; // 弾速

  // --- 自動カバー管理 ---
  bool isAutoCovering_ = false; // 自動カバー中フラグ
  const float kAutoCoverDistance_ = 8.0f; // 自動カバーを発動する接近距離（8m）

  // --- アニメーション用 ---
  const float kTargetFallSpeed_ = 4.0f; // 倒れる角速度（0.4秒で倒れ切る）
  const float kTargetRiseSpeed_ = 2.0f; // 起き上がる角速度（0.8秒で起き上がる）
  const float kTargetWaitDuration_ = 2.0f; // 倒れてから復帰するまでの待ち時間（2秒）

  // ImGui用のフラグ
  bool isUpdateCameraSettings_ = false;

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
  const std::string controlPath_ = "ui/control.png";
  const std::string crosshairPath_ = "ui/crosshair.png";
};