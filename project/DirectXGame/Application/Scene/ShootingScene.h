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

    // ゲームオーバー/クリア/入場演出フェーズ
    enum class Phase {
        IntroConstruct,   // 入場演出（逆順展開：ワイヤーフレーム出現 ＋ ソリッド構築）
        Playing,          // 通常プレイ
        SwarmBattle,      // レール終点でのスウォーム演出/戦闘
        GameOverVignette, // ビネットで覆っていく（ゲームオーバー）
        GameOverWait,     // 暗転維持
        RestartSmoothing, // リセット後スムージングを徐々に消す
        ClearVignette,    // クリア時ビネット暗転
        ReturnToTitleVignette, // ESCで目を閉じる退場暗転
    };

private:
    // 入場演出用の進行度算出描画ヘルパー (逆順：ワイヤーフレーム出現 -> 骨格維持 -> ソリッド構築)
    void DrawObjectWithIntroTransition(Object3d *obj, float startTime, float duration);

    // 敵の出現位置を計算する関数
    Vector3 CalculateRingRotation(const Vector3 &direction);

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
        bool isNextHitShot = false; // 次回射撃が必中弾かどうか (予兆リング表示用)
        Vector3 initialOffsetPosition = { 0.0f, 0.0f, 0.0f }; // カメライン前の初期画面外位置
        float slideTimer = 0.0f; // スライドイン進行時間
        float slideDelay = 0.0f; // スライドイン開始ディレイ時間
        float slideDuration = 1.0f; // スライドイン所要時間
        float deathTimer = 0.0f; // 撃破後ディゾルブ消滅タイマー (0.0:生存 -> 0.0〜1.0:消滅中 -> 1.0:完全消滅)
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
    Vector3 targetBasePos_ = { 0.0f, 2.0f, 10.0f };
    bool isHit_ = false;
    float cameraBasePitch_ = 0.0f; // カメラの基準X軸回転
    float cameraYaw_ = 0.0f;       // カメラのY軸回転（ラジアン、90度ずつ変化）
    float cameraBaseRoll_ = 0.0f;  // カメラの基準Z軸回転
    float cameraFovY_ = 45.0f;     // カメラのFOV

    // パーティクル設定など
    int currentBlendMode_ = 1; // NormalBlend
    bool isShowMaterial_ = true;
    bool isShowSprite_ = true;
    bool isShowSkybox_ = true;

    // プロジェクタイル管理
    std::vector<std::unique_ptr<EnemyProjectile>> projectiles_;
    float speed_ = 1.2f;                            // 通常弾の速度 (少し速め)
    float engineerProjectileSpeed_ = 0.18f;          // 工兵爆発弾（ランチャー）の速度 (ロケット弾イメージで遅め)
    float jammingProjectileSpeed_ = 1.8f;           // ジャミング弾の速度 (弾の中で最速)
    int coverExplosionHits_ = 0;                    // カバー時の爆発弾ヒット蓄積数 (2回で1ライフ減少)
    float lifeBlinkTimer_ = 0.0f;                   // カバー被弾時点滅タイマー
    float projectileSpawnTimer_ = 0.0f;
    const float kProjectileSpawnInterval = 120.0f;   // 通常敵の発射間隔 (約2秒)
    const float kEngineerSpawnInterval = 200.0f;     // 工兵の発射間隔 (約3.3秒、ロケット弾イメージで遅め)
    const float kChargeDuration = 36.0f;             // 予兆リング表示・縮小時間 (0.6秒 = 36フレーム)

    // ダメージ効果（グレースケール）
    float damageEffectStrength_ = 0.0f;

    // カメラシェイク
    float cameraShakeTimer_ = 0.0f;
    float cameraShakeDuration_ = 0.3f;
    float cameraShakeIntensity_ = 0.0f;

    // ヒットポイントとゲームオーバー管理
    int hitCount_ = 0;
    static constexpr int kMaxHits = 3;

    Phase phase_ = Phase::IntroConstruct;
    float phaseTimer_ = 0.0f;

    // 入場演出（ワイヤーフレーム出現 ＋ 逆順展開）パラメータ
    static constexpr float kIntroConstructDuration = 2.5f; // 入場演出全体の長さ(秒)
    static constexpr float kBlinkDuration = 1.0f;          // 瞬き明転演出の長さ(秒)
    const Vector3 kTransitionEdgeColor = { 0.0f, 1.0f, 0.8f };  // ディゾルブ境界の発光色（シアン）
    const Vector4 kWireframeColor = { 0.0f, 0.9f, 1.0f, 1.0f }; // ワイヤーフレームの線色

    // ビネット演出パラメータ（徐々に強化）
    float vignetteScale_ = 16.0f;                      // 小さいほど暗くなる
    float vignetteExponent_ = 0.8f;                    // 大きいほど急激に暗くなる
    static constexpr float kVignetteInDuration = 1.5f; // 覆うのにかかる時間(秒)
    static constexpr float kGameOverWaitDuration = 3.0f; // 暗転維持時間(秒)
    static constexpr float kReturnToTitleDuration = 0.8f; // タイトル戻り時の目を閉じる時間(秒)

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
    const float kCameraSpeed = 0.35f;

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

    // ESCキー案内スプライト
    std::unique_ptr<Sprite> escTextSprite_;
    Vector2 escTextPos_ = { 30.0f, 85.0f };
    Vector2 escTextScale_ = { 160.0f, 40.0f };

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
