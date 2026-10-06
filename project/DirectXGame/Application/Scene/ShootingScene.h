#pragma once
#include "BaseScene.h"
#include "Camera.h"
#include "LevelLoader.h"
#include "Object3d.h"
#include "Skybox.h"
#include "Sprite.h"
#include <memory>
#include <vector>

#include "EnemyProjectile.h"
#include "PostProcessManager.h"
#include "RenderTexture.h"
#include "Swarm/SwarmManager.h"

class Model;

class ShootingScene : public BaseScene {
public:
  ShootingScene();
  ~ShootingScene() override;

  void Initialize() override;
  void Update() override;

  void Draw() override;
  void Finalize() override;

  // ゲームオーバー/クリア演出フェーズ
  enum class Phase {
    Playing,          // 通常プレイ
    SwarmBattle,      // レール終点でのスウォーム演出/戦闘
    GameOverVignette, // ビネットで覆っていく（ゲームオーバー）
    GameOverWait,     // 暗転維持
    RestartSmoothing, // リセット後スムージングを徐々に消す
    ClearVignette,    // クリア時ビネット暗転
  };

private:
  // シーン描画のヘルパー
  void DrawScene(Camera *camera);

#ifdef USE_IMGUI
  // ImGui操作の更新
  void UpdateImGui();
  // ImGuiでグローバル設定のパラメータを調整するための関数
  void UpdateImGui_GlobalSettings();
  // ImGuiでゲームカメラのパラメータを調整するための関数
  void UpdateImGui_GameCamera();
  // ImGuiでObject3dのパラメータを調整するための関数
  void UpdateImGui_Object3d();
  // ImGuiでSkyboxのパラメータを調整するための関数
  void UpdateImGui_Skybox();
  // ImGuiでパーティクルのパラメータを調整するための関数
  void UpdateImGui_Particle();
  // ImGuiでゲームの状態を確認するための関数
  void UpdateImGui_GameStatus();
#endif // USE_IMGUI

private:
  // カメラ
  std::unique_ptr<Camera> camera_;

  // スカイボックス
  std::unique_ptr<Skybox> skybox_;

  // フロアオブジェクト
  std::unique_ptr<Object3d> floorObject_;
  // フロア設定
  struct FloorSettings {
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
  };
  FloorSettings floorSettings_ = { {0.0f, 0.0f, 100.0f}, {0.0f, 0.0f, 0.0f}, {5.0f, 1.0f, 25.0f} };

  // 敵のタイプ（種類）
  enum class EnemyType {
    Normal,   // 通常タイプ (グローバル必中率, Normal/Jamming弾)
    Engineer  // 工兵タイプ (必中率0.75, Blast爆発弾のみ)
  };

  // 敵情報構造体
  struct EnemyInfo {
    std::unique_ptr<Object3d> object;
    std::unique_ptr<Model> model; // 個別モデル
    Vector3 basePosition;
    Vector3 baseRotation;
    float distance = 0.0f;
    bool isActive = false;
    bool isDead = false;
    float shootTimer = 0.0f;
    float spawnTimer = 0.0f; // 出現タイマー
    int shootCount = 0; // 射撃回数カウンター
    int hitShotCount = 0; // 必中弾カウンター (属性弾ローテーション用)
    EnemyType type = EnemyType::Normal; // 敵のタイプ
  };

  // レベルから読み取った敵オブジェクト群
  std::vector<EnemyInfo> enemies_;

  // スウォーム（空中群体制御）システム
  std::unique_ptr<SwarmManager> swarmManager_;
  SwarmManager::AttackCommand pendingSwarmAttack_;
  bool hasPendingSwarmAttack_ = false;
  float swarmBattleTimer_ = 0.0f;

  // 照準（スプライト）
  std::unique_ptr<Sprite> crosshair_;

  // オフスクリーン用分岐戦闘の際には記述する

  // ビューのインデックス（0: Main）
  int mainViewIndex_ = 0;

  // ゲームロジック用変数
  float targetTimer_ = 0.0f;
  float orbitAngle_ = 0.0f;
  const float orbitRadius_ = 15.0f;
  Vector3 targetBasePos_ = {0.0f, 2.0f, 10.0f};
  bool isHit_ = false;
  float cameraBasePitch_ = 0.0f; // カメラの基準X軸回転
  float cameraYaw_ = 0.0f;       // カメラのY軸回転（ラジアン、90度ずつ変化）
  float cameraBaseRoll_ = 0.0f;  // カメラの基準Z軸回転

  // パーティクル設定など
  int currentBlendMode_ = 1; // NormalBlend
  bool isShowMaterial_ = true;
  bool isShowSprite_ = true;
  bool isShowSkybox_ = true;

  // プロジェクタイル管理
  std::vector<std::unique_ptr<EnemyProjectile>> projectiles_;
  float speed_ = 0.45f;                            // 通常弾の速度 (少し速め)
  float engineerProjectileSpeed_ = 0.18f;          // 工兵爆発弾の速度 (ロケット弾イメージで遅め)
  float projectileSpawnTimer_ = 0.0f;
  const float kProjectileSpawnInterval = 120.0f;   // 通常敵の発射間隔 (約2秒)
  const float kEngineerSpawnInterval = 200.0f;     // 工兵の発射間隔 (約3.3秒、ロケット弾イメージで遅め)

  // ダメージ効果（グレースケール）
  float damageEffectStrength_ = 0.0f;

  // カメラシェイク
  float cameraShakeTimer_ = 0.0f;
  float cameraShakeDuration_ = 0.3f;
  float cameraShakeIntensity_ = 0.0f;

  // ヒットポイントとゲームオーバー管理
  int hitCount_ = 0;
  static constexpr int kMaxHits = 3;

  Phase phase_ = Phase::Playing;
  float phaseTimer_ = 0.0f;

  // ビネット演出パラメータ（徐々に強化）
  float vignetteScale_ = 16.0f;                      // 小さいほど暗くなる
  float vignetteExponent_ = 0.8f;                    // 大きいほど急激に暗くなる
  static constexpr float kVignetteInDuration = 1.5f; // 覆うのにかかる時間(秒)
  static constexpr float kGameOverWaitDuration = 3.0f; // 暗転維持時間(秒)

  // スムージング演出パラメータ
  float smoothingKernel_ = 31.0f;                      // 最大カーネルサイズ
  static constexpr float kSmoothingOutDuration = 2.0f; // フェードアウト時間(秒)
  static constexpr float kDeltaTime = 1.0f / 60.0f;

  // パス定数
  // 3Dモデルのファイルパス
  const std::string enemyModelDirectory_ = "Resources/Assets/Models/ShootingScene/enemy";
  const std::string enemyModel_ = "enemy.obj";
  const std::string bulletModelDirectory_ = "Resources/Assets/Models/ShootingScene/bullet";
  const std::string bulletModel_ = "bullet.obj";
  const std::string floorModelDirectory_ = "Resources/Assets/Models/ShootingScene/floor";
  const std::string floorModel_ = "floor.obj";
  // テクスチャファイルパスを保持
  const std::string crosshairPath_ = "ui/crosshair.png";
  const std::string ringParticleGroupName_ = "RingShapeGroup";

  // レベルデータ
  std::unique_ptr<LevelData> levelData_;

  // レール移動用
  std::vector<LevelData::BezierControlPoint> railPoints_;
  float cameraProgress_ = 0.0f;
  float maxProgress_ = 190.0f;
  bool isMovementPaused_ = false;
  const float kCameraSpeed = 0.5f;

  // マガジン・隠れるアクション用
  int ammo_ = 9;
  static constexpr int kMaxAmmo = 9;
  bool isCovering_ = false;
  float coverYOffset_ = 0.0f;
  float coverYTarget_ = -0.2f;
  float reloadTimer_ = 0.0f;
  static constexpr float kReloadDuration = 1.0f;

  // レール座標計算用ヘルパー関数
  Vector3 CalculateRailPosition(float progress);

  // UI用スプライト
  // ライフUI
  std::unique_ptr<Sprite> lifeBg_;
  std::vector<std::unique_ptr<Sprite>> lifeUnits_;
  std::vector<float> lifeCurrentX_;
  std::vector<float> lifeTargetX_;

  // 弾薬UI
  std::unique_ptr<Sprite> ammoBg_;
  std::vector<std::unique_ptr<Sprite>> ammoUnits_;
  std::vector<float> ammoCurrentX_;
  std::vector<float> ammoTargetX_;
  int lastReloadCount_ = 0;

  // プログレスバーUI
  std::unique_ptr<Sprite> progressBg_;
  std::unique_ptr<Sprite> progressBar_;

  // スコア・タイマーUI
  int score_ = 0;
  float gameTimer_ = 60.0f;
  std::unique_ptr<Sprite> scoreBg_;
  std::vector<std::unique_ptr<Sprite>> scoreUnits_;
  std::unique_ptr<Sprite> timerBg_;
  std::vector<std::unique_ptr<Sprite>> timerUnits_;
  std::unique_ptr<Sprite> timerDot_;

  // カバー演出UI
  std::unique_ptr<Sprite> coverOverlay_;

  // ポーズ機能とデバッグ用ポーズ
  bool isPaused_ = false;
  int prevPostEffectMode_ = 0;
  bool isDebugPaused_ = false;
  std::vector<float> sectionProgresses_;
  int currentSectionIndex_ = 0;
  bool isDebugInfiniteAmmo_ = false;
  bool isDebugInvincible_ = false;
  float sectionJumpInvincibleTimer_ = 0.0f;
  static constexpr float kSectionJumpInvincibleDuration = 3.0f;

  // Radial Blur爆風演出用
  float radialBlurStrength_ = 0.0f;
  float radialBlurTimer_ = 0.0f;
  static constexpr float kRadialBlurDuration = 0.4f;

  // ランダムノイズ演出用
  float randomNoiseStrength_ = 0.0f;
  float randomNoiseTimer_ = 0.0f;

  // 演出弾・必中弾の撃ち分け設定
  float hitShotRate_ = 0.35f;      // 必中弾の割合 (0.0f: 全て演出弾 〜 1.0f: 全て必中弾)
  float missShotSpread_ = 3.0f;    // 演出弾の散布オフセット半径 (プレイヤー周辺を掠める範囲)

  // 区間ジャンプ処理
  void JumpToSection(int index);

  // レール移動の更新
  void UpdateRailMovement();

  // プレイヤーが無敵状態か判定
  bool IsInvincible() const;
};
