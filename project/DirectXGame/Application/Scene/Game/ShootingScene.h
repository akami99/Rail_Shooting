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
#include "EffectEditor.h"
#include "ShootingDemoAI.h"
#include "ShootingEffectManager.h"
#include "ShootingGameOverOverlay.h"
#include "ShootingHUD.h"
#include "ShootingPlayerController.h"
#include "ShootingTransitionDirector.h"
#include "ShootingWaveManager.h"
#include "StageRailManager.h"
#include "PostProcessManager.h"
#include "RenderTexture.h"
#include "Swarm/SwarmManager.h"

class Model;

class ShootingScene : public BaseScene, public IShootingEventObserver {
public:
    ShootingScene();
    ~ShootingScene() override;

    void Initialize() override;
    void Update() override;

    void Draw() override;
    void DrawForeground() override;
    void Finalize() override;

    void OnScoreEarned(int scoreDelta) override { score_ += scoreDelta; }

    // ゲームオーバー/クリア/入場演出フェーズ
    enum class Phase {
        Intro,              // 入場演出（Construct -> FastForward -> SwarmShowcase -> Rewind -> ReadyGo）
        Playing,            // 通常プレイ
        BossIntro,          // ボス登場時の演出
        SwarmBattle,        // レール終点でのスウォーム演出/戦闘
        GameOver,           // ゲームオーバー
        RestartSmoothing,   // リセット後スムージングを徐々に消す
        BossDefeatLookDown, // ボス撃破後：残骸の散らばる地面へカメラを向け、一斉ディゾルブ
        ClearVignette,      // クリア時演出：地面ワイヤーフレーム+ディゾルブ＆瞬き・目閉じ暗転
        ReturnToTitleVignette, // ESCで目を閉じる退場暗転
    };

private:
#ifdef USE_IMGUI
    // ImGui操作の更新
    void UpdateImGui();
    // ImGuiでグローバル設定のパラメータを調整するための関数
    void UpdateImGui_GlobalSettings();
    // ImGuiでゲームカメラのパラメータを調整するための関数
    void UpdateImGui_GameCamera();
    // ImGuiでスタート演出・終点スウォームアンカーのパラメータを調整するための関数
    void UpdateImGui_IntroSettings();
    // ImGuiでObject3dのパラメータを調整するための関数
    void UpdateImGui_Object3d();
    // ImGuiでSkyboxのパラメータを調整するための関数
    void UpdateImGui_Skybox();
    // ImGuiでパーティクルのパラメータを調整するための関数
    void UpdateImGui_Particle();
    // ImGuiでゲームの状態を確認するための関数
    void UpdateImGui_GameStatus();
    // スウォームドローン1機分のコリジョンサイズ確認ウィンドウ (Oキーで切り替え)
    void UpdateImGui_SwarmCollisionDebug();
#endif // USE_IMGUI

private:
    // カメラ
    std::unique_ptr<Camera> camera_;

    // スカイボックス
    std::unique_ptr<Skybox> skybox_;
    std::string skyboxTilePath_ = "tile.png";
    Vector2 skyboxUvScale_ = { 20.0f, 20.0f };

    // ステージレール・地形マネージャ
    std::unique_ptr<StageRailManager> railManager_;

    // 敵ウェーブ・プロジェクタイル・スウォーム/ボス戦マネージャ
    std::unique_ptr<ShootingWaveManager> waveManager_;

    // プレイヤー・武器制御マネージャ
    std::unique_ptr<ShootingPlayerController> playerController_;

    // 入場・クリア・退場遷移演出マネージャ
    std::unique_ptr<ShootingTransitionDirector> transitionDirector_;

    // ゲームオーバー画面オーバーレイ
    std::unique_ptr<ShootingGameOverOverlay> gameOverOverlay_;

    // 照準が攻撃対象に重なっているか判定
    bool CheckCrosshairAiming(const Vector2& mousePos) const;

    // オフスクリーン用分岐戦闘の際には記述する

    // ビューのインデックス（0: Main）
    int mainViewIndex_ = 0;

    // ゲームロジック用変数
    float targetTimer_ = 0.0f;
    float orbitAngle_ = 0.0f;
    const float orbitRadius_ = 15.0f;
    Vector3 targetBasePos_ = { 0.0f, 2.0f, 10.0f };
    bool isHit_ = false;
    float cameraBasePitch_ = 0.0f; // カメラの基準X軸回転
    float cameraYaw_ = 0.0f;       // カメラのY軸回転（ラジアン、90度ずつ変化）
    float cameraBaseRoll_ = 0.0f;  // カメラの基準Z軸回転
    float cameraFovY_ = 45.0f;     // カメラのFOV

    // レール進行方向と戦闘正面のカメラ向き制御
    float baseCombatPitch_ = 0.0f;           // 戦闘時の基準Pitch
    float baseCombatYaw_ = 0.0f;             // 戦闘時の基準Yaw
    float lookAheadDist_ = 1.0f;             // 進行方向算出の先読み距離 (m)
    float battleTransitionDistance_ = 6.0f;  // 戦闘区画手前で正面を向き始める距離 (m)
    float cameraRotateSpeed_ = 0.08f;        // 進行方向・正面への追従旋回速度
    std::vector<bool> sectionLookDirectionFlags_; // 区間ごとの進行方向追従フラグ (true: 進行方向, false: 正面固定)

    // パーティクル設定など
    int currentBlendMode_ = 1; // NormalBlend
    bool isShowMaterial_ = true;
    bool isShowSprite_ = true;
    bool isShowSkybox_ = true;

    float lifeBlinkTimer_ = 0.0f;                   // ライフ点滅タイマー

    // ダメージ効果（グレースケール）
    float damageEffectStrength_ = 0.0f;

    // カメラシェイク
    float cameraShakeTimer_ = 0.0f;
    float cameraShakeDuration_ = 0.3f;
    float cameraShakeIntensity_ = 0.0f;

    // ヒットポイントとゲームオーバー管理
    static constexpr int kDirectDamage = 10;
    static constexpr int kEngineerBlastDamage = 2;
    static constexpr int kBossGroupBlastDamage = 3;
    static constexpr int kLifeSegments = 3;

    Phase phase_ = Phase::Intro;
    float phaseTimer_ = 0.0f;

    // ボス登場・撃破演出パラメータ
    static constexpr float kBossIntroTotalDuration = 6.3f;
    static constexpr float kBossDefeatLookDownDuration = 2.8f;
    float bossIntroStartYaw_ = 0.0f;
    float bossIntroStartPitch_ = 0.0f;
    void StartBossIntro();

    float gameOverTimer_ = 20.0f;

    // パス定数
    // 3Dモデルのファイルパス
    const std::string enemyModelDirectory_ = "Resources/Assets/Models/ShootingScene/enemy";
    const std::string enemyModel_ = "enemy.obj";
    const std::string bulletModelDirectory_ = "Resources/Assets/Models/ShootingScene/bullet";
    const std::string bulletModel_ = "bullet.obj";

    // レベルデータ
    std::unique_ptr<LevelData> levelData_;

    // UI管理（ライフ・スコア・タイマー・プログレスバー・案内・Ready/Go・カバー演出）
    std::unique_ptr<ShootingHUD> hud_;

    // スコア・タイマー状態
    int score_ = 0;
    float gameTimer_ = 60.0f;

    // ポーズ機能とデバッグ用ポーズ
    bool isPaused_ = false;
    int prevPostEffectMode_ = 0;
    bool isDebugPaused_ = false;
    std::vector<float> sectionProgresses_;
    int currentSectionIndex_ = 0;

    // Radial Blur爆風演出用
    float radialBlurStrength_ = 0.0f;
    float radialBlurTimer_ = 0.0f;
    static constexpr float kRadialBlurDuration = 0.4f;

    // ランダムノイズ演出用
    float randomNoiseStrength_ = 0.0f;
    float randomNoiseTimer_ = 0.0f;

    // 区間ジャンプ処理
    void JumpToSection(int index);

    // レール移動の更新
    void UpdateRailMovement();

    // ゲームオーバー演出開始との要因受け渡し
    void StartGameOver(ShootingGameOverOverlay::DeathReason reason);

    // 各ウェーブ区間のクリア状況を算出
    std::vector<bool> ComputeSectionCleared() const;

    // デモプレイ機能
    std::unique_ptr<ShootingDemoAI> demoAI_;
    void UpdateDemoAI();
    Vector2 GetActiveMousePos() const;

    // デバッグ・確認用自動クリア機能
    bool isAutoClearing_ = false;
    float autoClearAttackTimer_ = 0.0f;
    void StartAutoClear();

#ifdef USE_IMGUI
    // スウォームドローン1機コリジョン確認モード (Oキーで切り替え)
    bool isSwarmCollisionDebug_ = false;
    std::unique_ptr<Object3d> debugDroneObj_;
    std::unique_ptr<Object3d> debugDroneObj2_;
    std::unique_ptr<Object3d> debugCollisionSphereObj_;
    std::unique_ptr<Object3d> debugCollisionSphereObj2_;
    float debugCollisionDist_ = 3.5f;
    float debugDroneRotY_ = 0.0f;
    bool debugAutoRotate_ = true;
    bool debugShowSecondDrone_ = false;
#endif // USE_IMGUI

    // エフェクトマネージャ・エフェクトエディタツール
    std::unique_ptr<ShootingEffectManager> effectManager_;
    std::unique_ptr<EffectEditor> effectEditor_;
};
