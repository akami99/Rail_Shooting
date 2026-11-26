#include "ShootingScene.h"
#include "BlendMode.h"
#include "DX12Context.h"
#include "Input.h"
#include "MathUtils.h"
#include "Model.h"
#include "ModelManager.h"
#include "MyGame.h"
#include "Object3dCommon.h"
#include "ParticleManager.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "Win32Window.h"
#include "SceneManager.h"
#include "ApplicationConfig.h"
#include "GameSession.h"
#include <cmath>
#include <numbers>

using namespace MathUtils;
using namespace BlendMode;

namespace {
Vector2 WorldToScreen(const Vector3& worldPos, Camera* camera) {
  if (!camera) return { 640.0f, 360.0f };
  const Matrix4x4& matViewProj = camera->GetViewProjectionMatrix();
  float x = worldPos.x * matViewProj.m[0][0] + worldPos.y * matViewProj.m[1][0] + worldPos.z * matViewProj.m[2][0] + matViewProj.m[3][0];
  float y = worldPos.x * matViewProj.m[0][1] + worldPos.y * matViewProj.m[1][1] + worldPos.z * matViewProj.m[2][1] + matViewProj.m[3][1];
  float w = worldPos.x * matViewProj.m[0][3] + worldPos.y * matViewProj.m[1][3] + worldPos.z * matViewProj.m[2][3] + matViewProj.m[3][3];
  if (std::abs(w) > 0.0001f) {
    x /= w;
    y /= w;
  }
  float screenX = (x + 1.0f) * 0.5f * static_cast<float>(Win32Window::kClientWidth);
  float screenY = (1.0f - y) * 0.5f * static_cast<float>(Win32Window::kClientHeight);
  return { screenX, screenY };
}
}

Vector2 ShootingScene::GetActiveMousePos() const {
  if (GameSession::GetInstance().IsDemoMode() && demoAI_) {
    return demoAI_->GetMousePos();
  }
  POINT mousePos;
  GetCursorPos(&mousePos);
  ScreenToClient(Win32Window::GetInstance()->GetHwnd(), &mousePos);
  return { static_cast<float>(mousePos.x), static_cast<float>(mousePos.y) };
}

ShootingScene::ShootingScene() = default;
ShootingScene::~ShootingScene() = default;

void ShootingScene::Initialize() {
#ifdef USE_IMGUI
    ShowCursor(TRUE);
#else

    ShowCursor(FALSE);
#endif

  // レベルデータのロード
  levelData_ = LevelLoader::LoadFile("testScene");

  // レール・地形マネージャ初期化
  railManager_ = std::make_unique<StageRailManager>();
  railManager_->InitializeRail(levelData_.get());

  // プレイヤー・武器制御マネージャ初期化
  playerController_ = std::make_unique<ShootingPlayerController>();
  playerController_->Initialize("ui/crosshair.png");

  // カメラの初期化
  camera_ = std::make_unique<Camera>();
  camera_->Initialize();

  Vector3 startCamPos = railManager_->GetCameraRailPosition(railManager_->GetProgress(), playerController_->GetEyeHeight());
  camera_->SetTranslate(startCamPos);
  camera_->SetFovY(cameraFovY_ * (std::numbers::pi_v<float> / 180.0f));

  if (levelData_ && !levelData_->players.empty()) {
    const auto &spawn = levelData_->players[0];
    baseCombatPitch_ = spawn.rotation.x;
    baseCombatYaw_ = spawn.rotation.y;
    cameraBaseRoll_ = spawn.rotation.z;
  } else {
    baseCombatPitch_ = 0.0f;
    baseCombatYaw_ = 0.0f;
    cameraBaseRoll_ = 0.0f;
  }

  // スタート時はスタート演出に合わせて正面を向いたままにする
  cameraBasePitch_ = baseCombatPitch_;
  cameraYaw_ = baseCombatYaw_;
  camera_->SetRotate({cameraBasePitch_, cameraYaw_, cameraBaseRoll_});

  Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());

  // テクスチャの読み込み
  TextureManager::GetInstance()->LoadTexture("Particles/circle.png");

  // モデル読み込み
  ModelManager::GetInstance()->LoadModel(enemyModelDirectory_, enemyModel_);
  ModelManager::GetInstance()->LoadModel(bulletModelDirectory_, bulletModel_);

  // 環境マップを敵モデルに適用
  Model *enemyModel = ModelManager::GetInstance()->FindModel(enemyModel_);
  if (enemyModel) {
    enemyModel->SetEnvironmentCoefficient(1.0f); // 反射強度を最大（1.0f）に設定
  }

  // エフェクトの初期化
  effectManager_ = std::make_unique<ShootingEffectManager>();
  effectManager_->Initialize();

  // エフェクトエディタの初期化
  effectEditor_ = std::make_unique<EffectEditor>();
  effectEditor_->Initialize();

  // スカイボックスの初期化（タイリング用シェーダーで初期化）
  skybox_ = std::make_unique<Skybox>();
  skybox_->InitializeTile(skyboxTilePath_, skyboxUvScale_);
  skybox_->SetCamera(camera_.get());

  TextureManager::GetInstance()->LoadTexture("skybox.dds");
  Object3dCommon::GetInstance()->SetEnvironmentMap(
      TextureManager::GetInstance()->GetSrvIndex("skybox.dds"));

  // 入場・クリア・退場遷移演出マネージャ初期化
  transitionDirector_ = std::make_unique<ShootingTransitionDirector>();

  // 地形オブジェクトの初期化
  railManager_->InitializeTerrain(camera_.get(), transitionDirector_->GetTransitionEdgeColor());

  // 敵ウェーブ・プロジェクタイル・スウォーム/ボス戦マネージャ初期化
  waveManager_ = std::make_unique<ShootingWaveManager>();
  waveManager_->Initialize(levelData_.get(), *railManager_, camera_.get(),
                           enemyModelDirectory_, enemyModel_,
                           transitionDirector_->GetIntroSwarmOffset(),
                           transitionDirector_->GetIntroSwarmRadius(),
                           transitionDirector_->GetIntroSwarmCount());
  waveManager_->AddObserver(this);
  waveManager_->AddObserver(playerController_.get());

  // --- デバッグ用区間リストの構築 ---
  sectionProgresses_.clear();
  sectionProgresses_.push_back(0.0f); // スタート地点
  if (levelData_) {
    for (const auto &enemyData : levelData_->enemies) {
      // 重複しないように追加
      if (std::find(sectionProgresses_.begin(), sectionProgresses_.end(),
                    enemyData.distance) == sectionProgresses_.end()) {
        sectionProgresses_.push_back(enemyData.distance);
      }
    }
  }
  // 昇順ソート
  std::sort(sectionProgresses_.begin(), sectionProgresses_.end());
  currentSectionIndex_ = 0;
  isDebugPaused_ = false;

  // 区間ごとの進行方向追従設定を初期化
  // 区間1/区間5 : true (進行方向追従)
  sectionLookDirectionFlags_.assign(sectionProgresses_.size(), false);
  if (sectionLookDirectionFlags_.size() > 1) {
    sectionLookDirectionFlags_[1] = true; // 区間1有効
    sectionLookDirectionFlags_[5] = true; // 区間5有効
  }

  // 初期フェーズの設定（入場・逆順展開演出から開始）
  phase_ = Phase::Intro;
  phaseTimer_ = 0.0f;
  isAutoClearing_ = false;
  autoClearAttackTimer_ = 0.0f;

  // 入場演出開始時：画面全体を完全ブラックアウト（暗転）状態に設定
  transitionDirector_->InitializeBlackout();
  transitionDirector_->StartIntroConstruct();

  // -----------------------------------------------------------------
  // UIスプライトの初期化
  // -----------------------------------------------------------------
  TextureManager::GetInstance()->LoadTexture("white.png");

  // ゲームオーバー画面オーバーレイ初期化
  gameOverOverlay_ = std::make_unique<ShootingGameOverOverlay>();
  gameOverOverlay_->Initialize();

  // デモ用AI・案内UI初期化
  demoAI_ = std::make_unique<ShootingDemoAI>();
  demoAI_->Initialize();

  score_ = 0;
  gameTimer_ = 60.0f;

  // HUDの初期化
  hud_ = std::make_unique<ShootingHUD>();
  hud_->Initialize(sectionProgresses_, railManager_->GetMaxProgress(), kLifeSegments, gameTimer_);

#ifdef USE_IMGUI
  // スウォームコリジョン確認用デバッグオブジェクト初期化 (Oキーで切り替え)
  ModelManager::GetInstance()->LoadModel("Resources/Assets/Models/sphere", "sphere.obj");
  debugDroneObj_ = std::make_unique<Object3d>();
  debugDroneObj_->Initialize();
  debugDroneObj_->SetModel(enemyModel_);
  debugDroneObj_->SetCamera(camera_.get());

  debugDroneObj2_ = std::make_unique<Object3d>();
  debugDroneObj2_->Initialize();
  debugDroneObj2_->SetModel(enemyModel_);
  debugDroneObj2_->SetCamera(camera_.get());

  debugCollisionSphereObj_ = std::make_unique<Object3d>();
  debugCollisionSphereObj_->Initialize();
  debugCollisionSphereObj_->SetModel("sphere.obj");
  debugCollisionSphereObj_->SetCamera(camera_.get());

  debugCollisionSphereObj2_ = std::make_unique<Object3d>();
  debugCollisionSphereObj2_->Initialize();
  debugCollisionSphereObj2_->SetModel("sphere.obj");
  debugCollisionSphereObj2_->SetCamera(camera_.get());
#endif // USE_IMGUI

}

void ShootingScene::Update() {
  // 入力の更新は常に行う
  Input::GetInstance()->Update();

  // ポーズ（TAB）のトグル検出
  if (Input::GetInstance()->IsKeyTriggered(DIK_TAB)) {
    if (!isPaused_) {
      isPaused_ = true;
      prevPostEffectMode_ = PostProcessManager::GetInstance()->GetCurrentMode();
      PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeGaussianBlur);
      PostProcessManager::GetInstance()->SetGaussianBlurParams(7, 2.0f);
    } else {
      isPaused_ = false;
      PostProcessManager::GetInstance()->SetMode(prevPostEffectMode_);
    }
  }

  // ESCでタイトルへ戻る演出開始（目を閉じる退場暗転）
  if (Input::GetInstance()->IsKeyTriggered(DIK_ESCAPE)) {
      if (phase_ != Phase::ReturnToTitleVignette) {
          phase_ = Phase::ReturnToTitleVignette;
          phaseTimer_ = 0.0f;
          if (waveManager_) { waveManager_->ClearProjectiles(); }
          if (transitionDirector_) { transitionDirector_->BeginReturnToTitleVignette(); }
      }
  }

  // デモモード中の更新およびSPACEキーによるタイトル戻り
  if (GameSession::GetInstance().IsDemoMode()) {
    if (Input::GetInstance()->IsKeyTriggered(DIK_SPACE)) {
      GameSession::GetInstance().SetDemoMode(false);
      PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
      SceneManager::GetInstance()->ChangeScene("TITLE");
      return;
    }
    UpdateDemoAI();
  }

  // Rキーでシューティングシーン遷移直後にリセット
  if (Input::GetInstance()->IsKeyTriggered(DIK_R)) {
      PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
      SceneManager::GetInstance()->ChangeScene("SHOOTING");
      return;
  }

#ifdef USE_IMGUI
  // Oキーでスウォーム1機コリジョン確認モードを切り替え
  if (Input::GetInstance()->IsKeyTriggered(DIK_O)) {
    isSwarmCollisionDebug_ = !isSwarmCollisionDebug_;
  }

  // UI処理 (ImGuiの定義)
  UpdateImGui();

  if (isSwarmCollisionDebug_) {
    UpdateImGui_SwarmCollisionDebug();
    camera_->Update();
    railManager_->UpdateTerrain(mainViewIndex_, camera_.get());
    if (skybox_) {
      skybox_->Update(mainViewIndex_, camera_.get());
    }
    PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
    return;
  }
#endif // USE_IMGUI

  if (isPaused_ || isDebugPaused_) {
    camera_->Update();
#ifdef USE_IMGUI
    if (isDebugPaused_ && effectEditor_) {
      ParticleManager::GetInstance()->SetIsUpdate(effectEditor_->IsUpdateParticle());
      ParticleManager::GetInstance()->SetUseBillboard(effectEditor_->UseBillboard());
      ParticleManager::GetInstance()->Update(*camera_, ApplicationConfig::kDeltaTime);
    }
#endif // USE_IMGUI
    return;
  }

  // 現在の railManager_->GetProgress() から currentSectionIndex_ を更新
  for (int i = static_cast<int>(sectionProgresses_.size()) - 1; i >= 0; --i) {
    if (railManager_->GetProgress() >= sectionProgresses_[i]) {
      currentSectionIndex_ = i;
      break;
    }
  }

  // 無敵タイマーの更新
  if (playerController_) {
    playerController_->TickInvincibleTimer(ApplicationConfig::kDeltaTime);
  }

  // 制限時間の更新
  if (phase_ == Phase::Playing) {
    gameTimer_ -= ApplicationConfig::kDeltaTime;
    if (gameTimer_ <= 0.0f) {
      gameTimer_ = 0.0f;

      // タイムアップによるゲームオーバー遷移
      StartGameOver(ShootingGameOverOverlay::DeathReason::TimeUp);

      return; // ここでUpdateを抜けてゲームを一時停止させる
    }
  }

  const float kHalfPI = std::numbers::pi_v<float> / 2.0f;

  // 終点プレビュー固定モード (ImGui確認用)
  if (transitionDirector_ && transitionDirector_->UpdatePreviewMode(
          railManager_.get(), playerController_.get(), waveManager_.get(),
          camera_.get(), skybox_.get(), cameraBasePitch_, cameraYaw_, mainViewIndex_)) {
    return;
  }

  // =====================================================
  // フェーズ: 入場カットシーン演出群 (State Pattern)
  // =====================================================
  if (phase_ == Phase::Intro && transitionDirector_) {
    ShootingTransitionDirector::IntroContext introCtx{
        phaseTimer_,
        isPaused_,
        railManager_.get(),
        playerController_.get(),
        waveManager_.get(),
        hud_.get(),
        camera_.get(),
        skybox_.get(),
        cameraBasePitch_,
        cameraYaw_,
        cameraBaseRoll_,
        baseCombatPitch_,
        baseCombatYaw_,
        mainViewIndex_
    };
    bool introFinished = transitionDirector_->UpdateIntro(introCtx);
    if (transitionDirector_->NeedsCrosshairUpdateInIntro()) {
      Vector2 mPos = GetActiveMousePos();
      if (playerController_) { playerController_->UpdateCrosshair(mPos, CheckCrosshairAiming(mPos)); }
    }
    if (introFinished) {
      phase_ = Phase::Playing;
    }
    return;
  }

  // タイトル戻り（目を閉じる退場暗転）
  if (phase_ == Phase::ReturnToTitleVignette && transitionDirector_) {
    if (transitionDirector_->UpdateReturnToTitleVignette(phaseTimer_, railManager_.get(),
                                                         camera_.get(), skybox_.get(), mainViewIndex_)) {
      SceneManager::GetInstance()->ChangeScene("TITLE");
    }
    return;
  }

  // =====================================================
  // フェーズ: ゲームオーバー演出（カウントダウン・進捗UI表示・照準操作）
  // =====================================================
  if (phase_ == Phase::GameOver) {
    phaseTimer_ += ApplicationConfig::kDeltaTime;

    Vector2 mPos = GetActiveMousePos();
    if (playerController_) { playerController_->UpdateCrosshair(mPos, CheckCrosshairAiming(mPos)); }

    if (gameOverOverlay_) {
      bool isClick = GameSession::GetInstance().IsDemoMode()
          ? (demoAI_ && demoAI_->IsShootTriggered())
          : Input::GetInstance()->IsMouseButtonTriggered(0);
      auto goRes = gameOverOverlay_->Update(phaseTimer_, gameOverTimer_, mPos, isClick, hud_.get(), playerController_.get());
      if (goRes.updateCrosshair && playerController_) {
        playerController_->UpdateCrosshair(mPos, goRes.hoverAny);
      }
      if (goRes.requestRetry) {
        SceneManager::GetInstance()->ChangeScene("SHOOTING");
        return;
      }
      if (goRes.requestTitle) {
        SceneManager::GetInstance()->ChangeScene("TITLE");
        return;
      }
    }

    if (hud_ && waveManager_) {
      hud_->UpdateGameOverProgressBar(railManager_->GetProgress(), railManager_->GetMaxProgress(),
          sectionProgresses_, ComputeSectionCleared(), lifeBlinkTimer_);
      hud_->UpdateGameOverTimer(gameOverTimer_);
    }

    camera_->Update();
    railManager_->UpdateTerrain(mainViewIndex_, camera_.get());
    if (skybox_) skybox_->Update(mainViewIndex_, camera_.get());
    if (waveManager_) {
      waveManager_->UpdateAliveEnemiesTransforms(mainViewIndex_, camera_.get());
    }

    ParticleManager::GetInstance()->SetIsUpdate(true);
    ParticleManager::GetInstance()->SetUseBillboard(false);
    ParticleManager::GetInstance()->Update(*camera_, ApplicationConfig::kDeltaTime);
    return;
  }

  // =====================================================
  // フェーズ: ボス登場時の演出
  // =====================================================
  if (phase_ == Phase::BossIntro) {
    if (!isPaused_) {
      phaseTimer_ += ApplicationConfig::kDeltaTime;

      // SPACE / ENTER で演出スキップ
      if (Input::GetInstance()->IsKeyTriggered(DIK_SPACE) ||
          Input::GetInstance()->IsKeyTriggered(DIK_RETURN)) {
        phaseTimer_ = kBossIntroTotalDuration;
      }

      Vector3 basePos = railManager_->GetCameraRailPosition(railManager_->GetMaxProgress(), playerController_->GetEyeHeight());
      float baseYaw = (levelData_ && !levelData_->players.empty()) ? levelData_->players[0].rotation.y : 0.0f;
      Vector3 forward = { std::sin(baseYaw), 0.0f, std::cos(baseYaw) };
      Vector3 up = { 0.0f, 1.0f, 0.0f };

      railManager_->SetProgress(railManager_->GetMaxProgress());
      camera_->SetTranslate(basePos);

      // カメラ演出: 後ろ上向きに素早く振り返る -> 前方を向かせてボス戦画角まで上げる
      if (phaseTimer_ < 0.45f) {
        float p = phaseTimer_ / 0.45f;
        float ease = 1.0f - (1.0f - p) * (1.0f - p);
        cameraYaw_ = bossIntroStartYaw_ + ease * std::numbers::pi_v<float>;
        cameraBasePitch_ = bossIntroStartPitch_ + ease * (-0.22f - bossIntroStartPitch_);
      } else if (phaseTimer_ < 1.80f) {
        cameraYaw_ = bossIntroStartYaw_ + std::numbers::pi_v<float>;
        cameraBasePitch_ = -0.22f;
      } else if (phaseTimer_ < 3.00f) {
        float p = (phaseTimer_ - 1.80f) / 1.20f;
        float ease = p * p * (3.0f - 2.0f * p);
        float startYaw = bossIntroStartYaw_ + std::numbers::pi_v<float>;
        float targetYaw = baseYaw + 2.0f * std::numbers::pi_v<float>;
        cameraYaw_ = startYaw + ease * (targetYaw - startYaw);
        cameraBasePitch_ = -0.22f + ease * (-0.18f - (-0.22f));
      } else {
        cameraYaw_ = baseYaw;
        cameraBasePitch_ = -0.18f;
      }

      camera_->SetRotate({ cameraBasePitch_, cameraYaw_, cameraBaseRoll_ });
      camera_->Update();
      railManager_->UpdateTerrain(mainViewIndex_, camera_.get());
      if (skybox_) {
        skybox_->Update(mainViewIndex_, camera_.get());
      }

      if (waveManager_) {
        waveManager_->UpdateBossIntro(phaseTimer_, kBossIntroTotalDuration, basePos, baseYaw, camera_.get());
      }

      // 演出終了 -> ボス戦開始
      if (phaseTimer_ >= kBossIntroTotalDuration) {
        cameraYaw_ = baseYaw;
        cameraBasePitch_ = -0.18f;
        phase_ = Phase::SwarmBattle;
        if (waveManager_) {
          waveManager_->BeginSwarmBattle(basePos, baseYaw);
        }
      }
    }

    ParticleManager::GetInstance()->SetIsUpdate(true);
    ParticleManager::GetInstance()->SetUseBillboard(false);
    ParticleManager::GetInstance()->Update(*camera_, ApplicationConfig::kDeltaTime);
    return;
  }

  // =====================================================
  // フェーズ: スウォーム戦闘（約100機がプレイヤー周辺20m以内を旋回・突撃）
  // =====================================================
  if (phase_ == Phase::SwarmBattle && waveManager_) {
    waveManager_->AddSwarmBattleTimer(ApplicationConfig::kDeltaTime);

    if (waveManager_->IsBossDefeated()) {
      isAutoClearing_ = false;
      phase_ = Phase::BossDefeatLookDown;
      phaseTimer_ = 0.0f;
      damageEffectStrength_ = 0.0f;
      radialBlurTimer_ = 0.0f;
      randomNoiseTimer_ = 0.0f;
      PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
      PostProcessManager::GetInstance()->SetStrength(1.0f);
      waveManager_->ClearProjectiles();
      ParticleManager::GetInstance()->SetIsUpdate(true);
      ParticleManager::GetInstance()->SetUseBillboard(false);
      ParticleManager::GetInstance()->Update(*camera_, ApplicationConfig::kDeltaTime);
      return;
    }
  }

  // =====================================================
  // フェーズ: ボス撃破後（残骸の散らばる地面へカメラを向け、一斉ディゾルブ）
  // =====================================================
  if (phase_ == Phase::BossDefeatLookDown) {
    phaseTimer_ += ApplicationConfig::kDeltaTime;

    if (waveManager_) {
      Vector3 basePos = railManager_->GetCameraRailPosition(railManager_->GetMaxProgress(), playerController_->GetEyeHeight());
      float baseYaw = (levelData_ && !levelData_->players.empty()) ? levelData_->players[0].rotation.y : 0.0f;
      waveManager_->UpdateBossDefeatLookDown(phaseTimer_, ApplicationConfig::kDeltaTime, basePos, baseYaw, camera_.get(), cameraYaw_, cameraBasePitch_);
      camera_->SetRotate({ cameraBasePitch_, cameraYaw_, cameraBaseRoll_ });
    }

    camera_->Update();
    railManager_->UpdateTerrain(mainViewIndex_, camera_.get());
    if (skybox_) {
      skybox_->Update(mainViewIndex_, camera_.get());
    }
    ParticleManager::GetInstance()->SetIsUpdate(true);
    ParticleManager::GetInstance()->SetUseBillboard(false);
    ParticleManager::GetInstance()->Update(*camera_, ApplicationConfig::kDeltaTime);

    if (phaseTimer_ >= kBossDefeatLookDownDuration) {
      if (waveManager_) {
        waveManager_->ClearSwarmDrones();
        waveManager_->ClearAmbientSwarmDrones();
      }
      phase_ = Phase::ClearVignette;
      phaseTimer_ = 0.0f;
      if (transitionDirector_) {
        transitionDirector_->BeginClearVignette();
      }
    }
    return;
  }

  // =====================================================
  // フェーズ: クリア演出（地面ワイヤーフレーム+ディゾルブ ＆ 瞬き1回・目閉じ暗転）
  // =====================================================
  if (phase_ == Phase::ClearVignette && transitionDirector_) {
    if (transitionDirector_->UpdateClearVignette(phaseTimer_, railManager_.get(), camera_.get(), skybox_.get(), mainViewIndex_)) {
      GameSession::GetInstance().SetClearResult(score_, gameTimer_);
      SceneManager::GetInstance()->ChangeScene("CLEAR");
    }
    return;
  }

  // リスタートスムージングフェードアウト
  if (phase_ == Phase::RestartSmoothing && transitionDirector_) {
    if (transitionDirector_->UpdateRestartSmoothing(phaseTimer_)) {
      phase_ = Phase::Playing;
    }
  }

  // =====================================================
  // 通常プレイ時のゲームロジック (Playing / RestartSmoothing共通)
  // =====================================================

  // カバー・リロード・武器切り替え更新
  if (playerController_) {
    playerController_->UpdateCoverAndWeapon(
        GameSession::GetInstance().IsDemoMode(),
        demoAI_ ? demoAI_->IsCovering() : false,
        camera_.get(),
        effectManager_.get());
  }

  // --- レール移動の更新 ---
  UpdateRailMovement();

  // カメラシェイクオフセットの計算（被弾時の画面揺れ）
  Vector3 shakeOffset = {0.0f, 0.0f, 0.0f};
  Vector3 shakeRotOffset = {0.0f, 0.0f, 0.0f};
  if (cameraShakeTimer_ > 0.0f) {
    cameraShakeTimer_ -= ApplicationConfig::kDeltaTime;
    if (cameraShakeTimer_ < 0.0f) cameraShakeTimer_ = 0.0f;
    float t = (cameraShakeDuration_ > 0.0f) ? (cameraShakeTimer_ / cameraShakeDuration_) : 0.0f;
    float currentIntensity = cameraShakeIntensity_ * (t * t); // 二次関数イージング減衰

    float rx = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f);
    float ry = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f);
    float rz = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f);
    float rRot = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f);

    shakeOffset = {rx * currentIntensity, ry * currentIntensity, rz * (currentIntensity * 0.4f)};
    shakeRotOffset = {rx * currentIntensity * 0.06f, ry * currentIntensity * 0.06f, rRot * currentIntensity * 0.10f};
  }

  Vector3 camPos = railManager_->GetCameraRailPosition(railManager_->GetProgress(), playerController_->GetEyeHeight());
  camPos.y += playerController_ ? playerController_->GetCoverYOffset() : 0.0f;
  camPos = Add(camPos, shakeOffset);
  camera_->SetTranslate(camPos);

#ifdef USE_IMGUI
  // A/Dキーでカメラ回転
  if (Input::GetInstance()->IsKeyDown(DIK_A)) {
    cameraYaw_ -= kHalfPI / 100.0f;
  }
  if (Input::GetInstance()->IsKeyDown(DIK_D)) {
    cameraYaw_ += kHalfPI / 100.0f;
  }
#endif // USE_IMGUI

  // レール移動中および戦闘区画へのアプローチ時のカメラ向き制御（区間別追従設定）
  if (phase_ == Phase::Playing || phase_ == Phase::RestartSmoothing) {
    // 現在の移動区間インデックスを判定
    int currentMovingSection = 0;
    for (int i = static_cast<int>(sectionProgresses_.size()) - 1; i >= 0; --i) {
      if (railManager_->GetProgress() >= sectionProgresses_[i]) {
        currentMovingSection = i;
        break;
      }
    }

    bool enableRailLookDirection = false;
    if (currentMovingSection >= 0 && currentMovingSection < static_cast<int>(sectionLookDirectionFlags_.size())) {
      enableRailLookDirection = sectionLookDirectionFlags_[currentMovingSection];
    }

    float targetYaw = baseCombatYaw_;
    float targetPitch = baseCombatPitch_;

    if (enableRailLookDirection) {
      // 進行方向ベクトル（現在のレール位置から少し先へのベクトル）
      float lookAhead = lookAheadDist_;
      Vector3 pNow = railManager_->CalculateRailPosition(railManager_->GetProgress());
      Vector3 pAhead = railManager_->CalculateRailPosition(std::min(railManager_->GetProgress() + lookAhead, railManager_->GetMaxProgress()));
      Vector3 moveDir = Subtract(pAhead, pNow);
      float moveLenSq = moveDir.x * moveDir.x + moveDir.z * moveDir.z;

      float forwardYaw = baseCombatYaw_;
      float forwardPitch = baseCombatPitch_;
      if (moveLenSq > 0.0001f) {
        forwardYaw = std::atan2(moveDir.x, moveDir.z);
        float horizDist = std::sqrt(moveLenSq);
        forwardPitch = std::clamp(std::atan2(-moveDir.y, horizDist) * 0.6f, -0.35f, 0.35f);
      }

      // 次の戦闘区画（停止地点）までの残り距離を計算
      float nextBattleDist = waveManager_
          ? waveManager_->GetNextBattleDistance(railManager_->GetProgress(), railManager_->GetMaxProgress())
          : railManager_->GetMaxProgress();

      // 戦闘区画に停止中か、到達間近かに応じたブレンド率 (0.0: 進行方向, 1.0: 正面)
      float blendFactor = 0.0f;
      if (railManager_->IsMovementPaused()) {
        blendFactor = 1.0f; // 戦闘停止中は完全に正面
      } else {
        float distToBattle = nextBattleDist - railManager_->GetProgress();
        if (distToBattle <= battleTransitionDistance_) {
          float t = 1.0f - (distToBattle / battleTransitionDistance_);
          t = std::clamp(t, 0.0f, 1.0f);
          blendFactor = t * t * (3.0f - 2.0f * t); // Smoothstep
        } else {
          blendFactor = 0.0f;
        }
      }

      // 目標角度の算出（進行方向から正面へ最短ブレンド）
      float diffYaw = forwardYaw - baseCombatYaw_;
      while (diffYaw < -std::numbers::pi_v<float>) diffYaw += 2.0f * std::numbers::pi_v<float>;
      while (diffYaw > std::numbers::pi_v<float>) diffYaw -= 2.0f * std::numbers::pi_v<float>;
      targetYaw = baseCombatYaw_ + diffYaw * (1.0f - blendFactor);
      targetPitch = baseCombatPitch_ + (forwardPitch - baseCombatPitch_) * (1.0f - blendFactor);
    }

    // 現在のカメラ角度を目標角度へスムーズに追従（最短経路）
    float deltaYaw = targetYaw - cameraYaw_;
    while (deltaYaw < -std::numbers::pi_v<float>) deltaYaw += 2.0f * std::numbers::pi_v<float>;
    while (deltaYaw > std::numbers::pi_v<float>) deltaYaw -= 2.0f * std::numbers::pi_v<float>;
    cameraYaw_ += deltaYaw * cameraRotateSpeed_;

    float deltaPitch = targetPitch - cameraBasePitch_;
    cameraBasePitch_ += deltaPitch * cameraRotateSpeed_;
  }

  // ボス戦（SwarmBattle）中はドローンがいる3区間（左・正面・右）に合わせてカメラ角度を切り替え
  if (phase_ == Phase::SwarmBattle && waveManager_) {
    float targetYaw = 0.0f;
    float targetPitch = 0.0f;
    if (waveManager_->GetBossCameraTarget(targetYaw, targetPitch)) {
      cameraYaw_ += (targetYaw - cameraYaw_) * 0.08f;
      cameraBasePitch_ += (targetPitch - cameraBasePitch_) * 0.08f;
    }
  }

  // 基準回転（basePitch, cameraYaw, baseRoll）にシェイクオフセットを加算（累積せず完全復元）
  camera_->SetRotate({
    cameraBasePitch_ + shakeRotOffset.x,
    cameraYaw_ + shakeRotOffset.y,
    cameraBaseRoll_ + shakeRotOffset.z
  });

  if (waveManager_) {
    Vector3 playerPos = railManager_->GetCameraRailPosition(railManager_->GetProgress(), playerController_->GetEyeHeight());
    waveManager_->UpdateEnemiesAndProjectiles(ApplicationConfig::kDeltaTime, railManager_->GetProgress(), playerPos, mainViewIndex_, camera_.get());
  }

  // スカイボックスの更新
  skybox_->Update(mainViewIndex_, camera_.get());

  // 地形オブジェクトの更新
  railManager_->UpdateTerrain(mainViewIndex_, camera_.get());

  // カメラとの当たり判定（プレイヤー被弾処理）
  bool isInvincible = playerController_ && playerController_->IsInvincible(isAutoClearing_);
  bool isForceInvincible = playerController_ && playerController_->IsForceInvincible(isAutoClearing_);
  if (waveManager_ && playerController_) {
    Vector3 targetPos = railManager_->CalculateRailPosition(railManager_->GetProgress());
    auto hitReport = waveManager_->ResolveProjectileHits(
        playerController_->IsCovering(), isInvincible, isForceInvincible,
        targetPos, camera_->GetTranslate(), playerController_->GetHp(), kDirectDamage);

    if (hitReport.coverBlastHits > 0) {
      radialBlurStrength_ = 0.04f;
      radialBlurTimer_ = kRadialBlurDuration;
      PostProcessManager::GetInstance()->SetRadialBlurParams(0.5f, 0.5f, radialBlurStrength_, 12);
      cameraShakeIntensity_ = 0.30f;
      cameraShakeDuration_ = 0.35f;
      cameraShakeTimer_ = 0.35f;

      bool isDead = playerController_->ApplyDamage(kEngineerBlastDamage * hitReport.coverBlastHits);
      damageEffectStrength_ = 1.0f;
      PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeGrayscale);
      if (isDead) {
        StartGameOver(ShootingGameOverOverlay::DeathReason::CoverBlast);
        return;
      }
    }

    if (hitReport.triggerDirectBlastFx) {
      radialBlurStrength_ = 0.05f;
      radialBlurTimer_ = kRadialBlurDuration;
      PostProcessManager::GetInstance()->SetRadialBlurParams(0.5f, 0.5f, radialBlurStrength_, 12);
      cameraShakeIntensity_ = 0.45f;
      cameraShakeDuration_ = 0.45f;
      cameraShakeTimer_ = 0.45f;
    } else if (hitReport.triggerDirectNormalFx) {
      cameraShakeIntensity_ = 0.20f;
      cameraShakeDuration_ = 0.25f;
      cameraShakeTimer_ = 0.25f;
    }

    if (hitReport.jammingHits > 0) {
      randomNoiseStrength_ = 0.7f;
      randomNoiseTimer_ = 5.0f;
      PostProcessManager::GetInstance()->SetRandomParams(randomNoiseStrength_);
    }

    int directTotal = hitReport.directBlastHits + hitReport.directNormalHits;
    if (directTotal > 0) {
      bool isDead = playerController_->ApplyDamage(kDirectDamage * directTotal);
      damageEffectStrength_ = 1.0f;
      PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeGrayscale);
      if (isDead) {
        StartGameOver(hitReport.directBlastHits > 0 ? ShootingGameOverOverlay::DeathReason::EngineerRocket : ShootingGameOverOverlay::DeathReason::NormalBullet);
        return;
      }
    }
  }

  // グレースケールダメージエフェクトの減衰（プレイ中のみ）
  if (damageEffectStrength_ > 0.0f) {
    damageEffectStrength_ -= 0.01f;
    if (damageEffectStrength_ < 0.0f) {
      damageEffectStrength_ = 0.0f;
      if (radialBlurTimer_ <= 0.0f && randomNoiseTimer_ <= 0.0f) {
        PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
      }
    }
  }
  PostProcessManager::GetInstance()->SetStrength(damageEffectStrength_);

  // Radial Blur爆風エフェクトの減衰と更新
  if (radialBlurTimer_ > 0.0f) {
    radialBlurTimer_ -= ApplicationConfig::kDeltaTime;
    if (radialBlurTimer_ <= 0.0f) {
      radialBlurTimer_ = 0.0f;
      radialBlurStrength_ = 0.0f;
      if (randomNoiseTimer_ > 0.0f) {
        PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeRandom);
      } else if (damageEffectStrength_ > 0.0f) {
        PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeGrayscale);
      } else {
        PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
      }
    } else {
      PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeRadialBlur);
      float t = radialBlurTimer_ / kRadialBlurDuration;
      float currentStrength = radialBlurStrength_ * (t * t);
      PostProcessManager::GetInstance()->SetRadialBlurParams(0.5f, 0.5f, currentStrength, 12);
    }
  }

  // Randomグリッチノイズエフェクトの減衰と更新
  if (randomNoiseTimer_ > 0.0f) {
    randomNoiseTimer_ -= ApplicationConfig::kDeltaTime;
    if (randomNoiseTimer_ <= 0.0f) {
      randomNoiseTimer_ = 0.0f;
      randomNoiseStrength_ = 0.0f;
      if (damageEffectStrength_ > 0.0f) {
        PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeGrayscale);
      } else {
        PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
      }
    } else {
      if (radialBlurTimer_ <= 0.0f) {
        PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeRandom);
        float t = randomNoiseTimer_ / 5.0f;
        if (t > 1.0f) t = 1.0f;
        float easeT = t * t;
        PostProcessManager::GetInstance()->SetRandomParams(randomNoiseStrength_ * easeT);
      }
    }
  }

  if (waveManager_) {
    waveManager_->RemoveDeadProjectiles();
  }

  // スプライト更新（照準をマウス位置に合わせる）
  Vector2 mPos = GetActiveMousePos();
  if (playerController_) { playerController_->UpdateCrosshair(mPos, CheckCrosshairAiming(mPos)); }
  camera_->Update();

  // 射撃（クリック / 連射）処理
  bool wantToShoot = playerController_ && playerController_->TryFire(
      GameSession::GetInstance().IsDemoMode(),
      demoAI_ && demoAI_->IsShootTriggered());

  if (wantToShoot) {

    // アモUI消費エフェクトをカメラの左下前方に発生させる
    Vector3 camPos = camera_->GetTranslate();
    Vector3 camRot = camera_->GetRotate();
    float sinY = std::sin(camRot.y);
    float cosY = std::cos(camRot.y);
    Vector3 forward = {sinY, 0.0f, cosY};
    Vector3 right = {cosY, 0.0f, -sinY};
    Vector3 up = {0.0f, 1.0f, 0.0f};
    Vector3 spawnPos =
        Add(camPos, Add(Multiply(1.5f, forward),
                        Add(Multiply(-0.4f, right), Multiply(-0.3f, up))));
    {
      // 残り弾薬UIの右端の上部分（スクリーン座標 X:208, Y:646）を現在の視野角・カメラ姿勢で逆投影して発生位置を決定
      const float sparkScreenX = 208.0f;
      const float sparkScreenY = 646.0f;
      float sparkNdcX = sparkScreenX / static_cast<float>(Win32Window::kClientWidth) * 2.0f - 1.0f;
      float sparkNdcY = 1.0f - sparkScreenY / static_cast<float>(Win32Window::kClientHeight) * 2.0f;
      Matrix4x4 invVPForSpark = Inverse(camera_->GetViewProjectionMatrix());
      Vector3 sparkNear = TransformPoint({sparkNdcX, sparkNdcY, 0.0f}, invVPForSpark);
      Vector3 sparkFar = TransformPoint({sparkNdcX, sparkNdcY, 1.0f}, invVPForSpark);
      Vector3 sparkDir = Normalize(Subtract(sparkFar, sparkNear));
      spawnPos = Add(camPos, Multiply(1.5f, sparkDir));
    }

    effectManager_->EmitAmmoSpark(spawnPos, 1);

    if (waveManager_) {
      waveManager_->ProcessPlayerShot(
          mPos,
          static_cast<float>(Win32Window::kClientWidth),
          static_cast<float>(Win32Window::kClientHeight),
          camera_.get(),
          railManager_->GetProgress(),
          effectManager_.get());
    }
  }

  // クリア判定
  if (railManager_->GetProgress() >= railManager_->GetMaxProgress() && waveManager_) {
    if (waveManager_->AreAllEnemiesDead() && effectManager_->AreClearEffectsFinished()) {
      if (phase_ == Phase::Playing) {
        StartBossIntro();
        return;
      }
    }
  }

  // デバッグ自動クリア：ボス戦中は自動でターゲットを狙って攻撃する
  if (isAutoClearing_ && phase_ == Phase::SwarmBattle && waveManager_) {
    Vector3 targetPos{};
    if (waveManager_->UpdateAutoClearBossAttack(ApplicationConfig::kDeltaTime, camera_.get(), autoClearAttackTimer_, targetPos)) {
      Vector2 targetScreenPos = WorldToScreen(targetPos, camera_.get());
      if (playerController_) {
        playerController_->UpdateCrosshair(targetScreenPos, true);
      }
    }
  }

  // 装飾用1000機スウォームの更新
  if (waveManager_) {
    Vector3 basePos = railManager_->GetCameraRailPosition(railManager_->GetMaxProgress(), playerController_->GetEyeHeight());
    float baseYaw = (levelData_ && !levelData_->players.empty()) ? levelData_->players[0].rotation.y : 0.0f;
    Vector3 forward = { std::sin(baseYaw), 0.0f, std::cos(baseYaw) };
    Vector3 ambientAnchor = {
        basePos.x + forward.x * 26.0f,
        basePos.y + 16.5f,
        basePos.z + forward.z * 26.0f
    };
    waveManager_->UpdateAmbientSwarm(ApplicationConfig::kDeltaTime, ambientAnchor, camera_.get());
  }

  // スウォーム（空中群体制御）のシミュレーション更新 (Compute Shader)
  if (waveManager_) {
    Vector3 camPos = camera_->GetTranslate();
    const bool isCovering = playerController_ && playerController_->IsCovering();
    auto swarmRes = waveManager_->UpdateSwarmSimulation(
        ApplicationConfig::kDeltaTime, camPos, camera_.get(), isCovering, effectManager_.get());

    if (swarmRes.groupBlastHits > 0 && !isForceInvincible && playerController_) {
      radialBlurStrength_ = 0.04f;
      radialBlurTimer_ = kRadialBlurDuration;
      PostProcessManager::GetInstance()->SetRadialBlurParams(0.5f, 0.5f, radialBlurStrength_, 12);
      cameraShakeIntensity_ = 0.30f;
      cameraShakeDuration_ = 0.35f;
      cameraShakeTimer_ = 0.35f;

      bool isDead = playerController_->ApplyDamage(kBossGroupBlastDamage * swarmRes.groupBlastHits);
      damageEffectStrength_ = 1.0f;
      PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeGrayscale);
      if (isDead) {
        StartGameOver(ShootingGameOverOverlay::DeathReason::DroneExplosion);
        return;
      }
    }
    if (swarmRes.directHits > 0 && !isForceInvincible && playerController_) {
      radialBlurStrength_ = 0.05f;
      radialBlurTimer_ = kRadialBlurDuration;
      PostProcessManager::GetInstance()->SetRadialBlurParams(0.5f, 0.5f, radialBlurStrength_, 12);
      cameraShakeIntensity_ = 0.45f;
      cameraShakeDuration_ = 0.45f;
      cameraShakeTimer_ = 0.45f;

      bool isDead = playerController_->ApplyDamage(kDirectDamage * swarmRes.directHits);
      damageEffectStrength_ = 1.0f;
      PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeGrayscale);
      if (isDead) {
        StartGameOver(ShootingGameOverOverlay::DeathReason::DroneExplosion);
        return;
      }
    }
    if (swarmRes.bossDefeated) {
      isAutoClearing_ = false;
      phase_ = Phase::BossDefeatLookDown;
      phaseTimer_ = 0.0f;
      damageEffectStrength_ = 0.0f;
      radialBlurTimer_ = 0.0f;
      randomNoiseTimer_ = 0.0f;
      PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
      PostProcessManager::GetInstance()->SetStrength(1.0f);
      waveManager_->ClearProjectiles();
      ParticleManager::GetInstance()->SetIsUpdate(true);
      ParticleManager::GetInstance()->SetUseBillboard(false);
      ParticleManager::GetInstance()->Update(*camera_, ApplicationConfig::kDeltaTime);
      return;
    }
  }

  // -----------------------------------------------------------------
  // UIスプライトの更新処理
  // -----------------------------------------------------------------
  // 1. ライフUIの更新
  {
    lifeBlinkTimer_ += ApplicationConfig::kDeltaTime;
    if (hud_ && playerController_) {
      hud_->UpdateLifeGauge(playerController_->GetHp(), ShootingPlayerController::kMaxHp, lifeBlinkTimer_);
    }
  }

  // 2. 弾薬・武器選択UI・ドロップアイコン演出の更新
  if (playerController_) {
    playerController_->UpdateUI(
        *camera_,
        static_cast<float>(Win32Window::kClientWidth),
        static_cast<float>(Win32Window::kClientHeight));
  }

  // 3. プログレスバー＆ウェーブ目盛り・アイコン・現在地マーカーの更新
  if (hud_) {
    hud_->UpdateGameplayProgressBar(railManager_->GetProgress(), railManager_->GetMaxProgress(),
        sectionProgresses_, ComputeSectionCleared(),
        phase_ == Phase::SwarmBattle, railManager_->IsMovementPaused(), lifeBlinkTimer_);
  }

  // 3.5 敵・ボス・プロジェクタイルHPゲージ更新
  if (waveManager_) {
    waveManager_->UpdateHpGauges(
        camera_.get(),
        static_cast<float>(Win32Window::kClientWidth),
        static_cast<float>(Win32Window::kClientHeight),
        railManager_->GetProgress(),
        phase_ == Phase::SwarmBattle);
  }

  // 4. カバー演出 / スコア / 案内 / タイマーUIの更新
  if (hud_) {
    hud_->UpdateScoreAndGuides(score_, playerController_ && playerController_->IsCovering());
    hud_->UpdateGameplayTimer(gameTimer_);
  }

  // パーティクルの更新
  if (effectEditor_) {
    ParticleManager::GetInstance()->SetIsUpdate(effectEditor_->IsUpdateParticle());
    ParticleManager::GetInstance()->SetUseBillboard(effectEditor_->UseBillboard());
  } else {
    ParticleManager::GetInstance()->SetIsUpdate(true);
    ParticleManager::GetInstance()->SetUseBillboard(false);
  }
  ParticleManager::GetInstance()->Update(*camera_, ApplicationConfig::kDeltaTime);
}

void ShootingScene::Draw() {
  // 描画設定
  Object3dCommon::GetInstance()->SetCommonDrawSettings(
      static_cast<BlendState>(currentBlendMode_));

  // メインカメラ (ViewIndex 0)
  // 1. 地形オブジェクト（床・エスカレーター・高台/坂道）の描画
  if (isShowMaterial_ && railManager_) {
    if (phase_ == Phase::Intro && transitionDirector_ && transitionDirector_->IsIntroConstructActive()) {
      railManager_->ForEachTerrainObject([this](Object3d* obj) {
        transitionDirector_->DrawObjectWithIntroTransition(obj, 0.0f, 1.4f, true, phaseTimer_, currentBlendMode_);
      });
    } else if (phase_ == Phase::ClearVignette && transitionDirector_) {
      railManager_->ForEachTerrainObject([this](Object3d* obj) {
        transitionDirector_->DrawObjectWithClearTransition(obj, 0.4f, 1.8f, true, phaseTimer_, currentBlendMode_);
      });
    } else {
      railManager_->DrawTerrain();
    }
  }

  // 2. 敵・敵弾・スウォーム群体の描画
  if (waveManager_) {
    waveManager_->DrawEnemiesAndProjectiles(railManager_->GetProgress(), isShowMaterial_, camera_.get());
  }

#ifdef USE_IMGUI
  // スウォーム1機コリジョン確認用プレビュー描画
  if (isSwarmCollisionDebug_) {
    // SwarmManager::Draw() が RootSignature/PSO を切り替えるため、Object3d 用に戻す
    Object3dCommon::GetInstance()->SetCommonDrawSettings(
        static_cast<BlendState>(currentBlendMode_));
    if (debugDroneObj_) debugDroneObj_->Draw(0);
    if (debugShowSecondDrone_ && debugDroneObj2_) debugDroneObj2_->Draw(0);

    Object3dCommon::GetInstance()->SetCommonDrawSettings(
        static_cast<BlendState>(currentBlendMode_), true);
    if (debugCollisionSphereObj_) {
      debugCollisionSphereObj_->DrawWireframe(0.0f, { 0.0f, 1.0f, 0.2f, 1.0f }, 0);
    }
    if (debugShowSecondDrone_ && debugCollisionSphereObj2_) {
      debugCollisionSphereObj2_->DrawWireframe(0.0f, { 1.0f, 0.8f, 0.0f, 1.0f }, 0);
    }
    Object3dCommon::GetInstance()->SetCommonDrawSettings(
        static_cast<BlendState>(currentBlendMode_), false);
  }
#endif // USE_IMGUI

  // 4.
  // 背景スカイボックスの描画（描画状態切り替えによる競合を防ぐため、常に不透明3Dの後に描画する）
  if (isShowSkybox_)
    skybox_->Draw(0);

  // パーティクルの描画
  BlendMode::BlendState particleBlend = effectEditor_
      ? effectEditor_->GetBlendMode()
      : BlendMode::BlendState::kBlendModeAdd;
  ParticleManager::GetInstance()->Draw(particleBlend);

  // スプライトの描画
  SpriteCommon::GetInstance()->SetCommonDrawSettings(
      static_cast<BlendState>(currentBlendMode_));

  // UIの下地・ゲージを非表示にする
  bool showUI = (phase_ == Phase::Playing || phase_ == Phase::SwarmBattle || phase_ == Phase::RestartSmoothing);

  if (showUI) {
    // 1. カバー演出 / 2. UIの下地
    if (hud_) {
      hud_->DrawCover(playerController_ && playerController_->IsCovering());
      hud_->DrawGameplayBackgrounds();
    }

    // UIのゲージ（メモリやバー）
    if (showUI) {
    if (hud_ && playerController_) {
      hud_->DrawLifeGauge(playerController_->GetHp(), ShootingPlayerController::kMaxHp);
    }
    if (playerController_) {
      playerController_->DrawAmmoAndWeaponUI();
    }

    } // end if (showUI for life & ammo)

    // プログレスバー＆ウェーブ目盛り・アイコン・現在地マーカー
    if (hud_) {
      hud_->DrawProgressBar();
    }

    if (showUI) {
    // 敵・ボス・プロジェクタイルのHPゲージ描画
    if (waveManager_) {
      waveManager_->DrawHpGauges(phase_ == Phase::SwarmBattle);
    }

    // エリート兵撃破時の弾薬ドロップ頭上アイコン描画
    if (playerController_) {
      playerController_->DrawDropEffects();
    }

    // スコア・案内表示スプライト
    if (hud_) {
      hud_->DrawScoreAndGuides();
    }

    } // end if (showUI for scores & enemies)

    // タイマー表示スプライト
    if (hud_) {
      hud_->DrawTimer(true);
    }
  }

  // スタート演出（Ready / Go 帯リール演出）
  if (phase_ == Phase::Intro && transitionDirector_ && transitionDirector_->IsIntroReadyGoActive() && hud_) {
    hud_->DrawReadyGo();
  }

  // 照準
  if (isShowSprite_ && showUI) {
    if (demoAI_) {
      demoAI_->DrawUI();
    }
    if (playerController_) {
      playerController_->DrawCrosshair();
    }
  }
}

void ShootingScene::DrawForeground() {
    // ゲームオーバー時以外は何もしない
    if (phase_ != Phase::GameOver) {
        return;
    }

    // スプライトの描画
    SpriteCommon::GetInstance()->SetCommonDrawSettings(
        static_cast<BlendState>(currentBlendMode_));

    // カバー演出・下地・進捗ゲージ・タイマー一式
    if (hud_) {
        hud_->DrawCover(playerController_ && playerController_->IsCovering());
        hud_->DrawGameOverBackgrounds();
        hud_->DrawProgressBar();
        hud_->DrawTimer(false);
    }

    // 選択肢（RETRY / TITLE 看板）・敗北要因テキスト
    if (gameOverOverlay_) {
        gameOverOverlay_->Draw();
    }

    if (demoAI_) {
        demoAI_->DrawUI();
    }

    // マウス照準スプライトを最前面に描画
    if (playerController_) {
        playerController_->DrawCrosshair();
    }
}

void ShootingScene::Finalize() {
  Object3dCommon::GetInstance()->SetDefaultCamera(nullptr);
  if (waveManager_) {
    waveManager_->RemoveObserver(this);
    waveManager_->RemoveObserver(playerController_.get());
    waveManager_->Finalize();
  }
  if (railManager_) {
    railManager_->Finalize();
  }
  skybox_.reset();
  hud_.reset();
  if (playerController_) {
    playerController_->Finalize();
  }
#ifdef USE_IMGUI
  debugDroneObj_.reset();
  debugDroneObj2_.reset();
  debugCollisionSphereObj_.reset();
  debugCollisionSphereObj2_.reset();
#endif // USE_IMGUI
  ParticleManager::GetInstance()->ClearAllParticles();
  if (phase_ != Phase::ClearVignette && phase_ != Phase::ReturnToTitleVignette) {
    PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
  }
}

void ShootingScene::JumpToSection(int index) {
  if (index < 0 || index >= static_cast<int>(sectionProgresses_.size()))
    return;

  isAutoClearing_ = false;
  autoClearAttackTimer_ = 0.0f;

  float targetProgress = sectionProgresses_[index];

  railManager_->SetProgress(targetProgress);
  currentSectionIndex_ = index;
  isDebugPaused_ = false; // ワープ後はポーズを自動解除
  if (playerController_) {
    playerController_->SetSectionJumpInvincible(ShootingPlayerController::kSectionJumpInvincibleDuration);
  }

  // 状態の復元（論理リセット）
  if (waveManager_) {
    int clearedScore = 0;
    waveManager_->ResetForJumpToSection(targetProgress, mainViewIndex_, camera_.get(), clearedScore);
    score_ = clearedScore;
  } else {
    score_ = 0;
  }
  orbitAngle_ = 0.0f;
  targetTimer_ = 0.0f;
  cameraBasePitch_ = 0.0f;
  cameraYaw_ = 0.0f;
  cameraBaseRoll_ = 0.0f;
  cameraShakeTimer_ = 0.0f;
  ParticleManager::GetInstance()->ClearAllParticles();
  isHit_ = false;
  damageEffectStrength_ = 0.0f;
  isPaused_ = false;
  score_ = 0;
  gameTimer_ = 60.0f;
  radialBlurStrength_ = 0.0f;
  radialBlurTimer_ = 0.0f;
  randomNoiseStrength_ = 0.0f;
  randomNoiseTimer_ = 0.0f;
  lifeBlinkTimer_ = 0.0f;

  if (playerController_) {
    playerController_->ResetState(true);
    playerController_->ResetAmmoUnitsLayout();
    if (hud_) {
      hud_->ResetLifeGauge(playerController_->GetHp(), ShootingPlayerController::kMaxHp);
    }
  }

  // カメラを初期位置に戻す
  Vector3 startCamPos = railManager_->GetCameraRailPosition(railManager_->GetProgress(), playerController_->GetEyeHeight());
  camera_->SetTranslate(startCamPos);
  cameraBasePitch_ = baseCombatPitch_;
  cameraYaw_ = baseCombatYaw_;
  camera_->SetRotate({cameraBasePitch_, cameraYaw_, cameraBaseRoll_});

  // カメラの一時停止フラグ（スタート地点以外は敵出現ポイントなので停止状態から始める）
  railManager_->SetMovementPaused((targetProgress > 0.0f));

  // スムージング演出をリスタート用に設定
  if (transitionDirector_) {
    transitionDirector_->BeginRestartSmoothing();
  }
  phase_ = Phase::RestartSmoothing;
  phaseTimer_ = 0.0f;
}

void ShootingScene::StartAutoClear() {
  // ポーズ解除
  isPaused_ = false;
  isDebugPaused_ = false;

  // 被弾・カバー・エフェクト状態のリセット
  if (playerController_) {
    playerController_->HealToFull();
    playerController_->ResetCover();
  }
  damageEffectStrength_ = 0.0f;
  radialBlurTimer_ = 0.0f;
  radialBlurStrength_ = 0.0f;
  randomNoiseTimer_ = 0.0f;
  randomNoiseStrength_ = 0.0f;
  cameraShakeTimer_ = 0.0f;
  cameraShakeIntensity_ = 0.0f;
  if (gameTimer_ <= 1.0f) {
    gameTimer_ = 60.0f;
  }

  // スタート演出状態の解除
  if (transitionDirector_) { transitionDirector_->ClearPreviewMode(); }
  if (hud_) { hud_->HideReadyGo(); }
  if (railManager_ && transitionDirector_) {
    railManager_->ClearTerrainDissolve(transitionDirector_->GetTransitionEdgeColor());
  }
  PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
  PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);

  // まだボス戦に入っていない場合は、スタート演出用スウォームを消去しボス戦開始位置へ進める
  if (!(phase_ == Phase::SwarmBattle && waveManager_ && waveManager_->IsBossMode())) {
    if (waveManager_) {
      waveManager_->KillAllForAutoClear();
    }
    effectManager_->ClearAll();
    effectManager_->StopClearWaitEffects();

    railManager_->SetProgress(railManager_->GetMaxProgress());
    railManager_->SetMovementPaused(false);
    camera_->SetTranslate(railManager_->GetCameraRailPosition(railManager_->GetMaxProgress(), playerController_->GetEyeHeight()));
    cameraBasePitch_ = baseCombatPitch_;
    cameraYaw_ = baseCombatYaw_;
    camera_->SetRotate({ cameraBasePitch_, cameraYaw_, cameraBaseRoll_ });

    phase_ = Phase::Playing;
    phaseTimer_ = 0.0f;
  }

  isAutoClearing_ = true;
  autoClearAttackTimer_ = 0.0f;
}

void ShootingScene::UpdateRailMovement() {
  if (waveManager_ && railManager_) {
    waveManager_->UpdateRailEnemyTriggers(*railManager_);
  }
}

bool ShootingScene::CheckCrosshairAiming(const Vector2& mPos) const {
  if (!playerController_ || playerController_->IsCovering() || !playerController_->HasCurrentWeaponAmmo() || !waveManager_) {
    return false;
  }
  float rayX = mPos.x / static_cast<float>(Win32Window::kClientWidth) * 2.0f - 1.0f;
  float rayY = 1.0f - mPos.y / static_cast<float>(Win32Window::kClientHeight) * 2.0f;
  Matrix4x4 invVP = Inverse(camera_->GetViewProjectionMatrix());
  Vector3 nearPos = TransformPoint({rayX, rayY, 0.0f}, invVP);
  Vector3 farPos = TransformPoint({rayX, rayY, 1.0f}, invVP);
  Vector3 rayDir = Normalize(Subtract(farPos, nearPos));
  return waveManager_->CheckAiming(nearPos, rayDir, railManager_->GetProgress());
}

void ShootingScene::StartGameOver(ShootingGameOverOverlay::DeathReason reason) {
  phase_ = Phase::GameOver;
  phaseTimer_ = 0.0f;
  damageEffectStrength_ = 0.0f;

  if (gameOverOverlay_) {
    gameOverOverlay_->Begin(reason, gameOverTimer_);
  }
  if (hud_) {
    hud_->ResetGameOverColors();
  }
  if (playerController_) {
    playerController_->SetForceCover(true);
  }
}

std::vector<bool> ShootingScene::ComputeSectionCleared() const {
  return waveManager_ ? waveManager_->ComputeSectionCleared(sectionProgresses_)
                      : std::vector<bool>(sectionProgresses_.size(), true);
}



void ShootingScene::UpdateDemoAI() {
  if (!demoAI_ || !GameSession::GetInstance().IsDemoMode()) return;

  ShootingDemoAI::Context ctx;
  ctx.camera = camera_.get();
  ctx.gameTimer = gameTimer_;
  ctx.isCovering = playerController_ && playerController_->IsCovering();
  ctx.playerHp = playerController_ ? playerController_->GetHp() : ShootingPlayerController::kMaxHp;
  ctx.directDamage = kDirectDamage;
  ctx.ammo = playerController_ ? playerController_->GetAmmo() : ShootingPlayerController::kMaxAmmo;
  ctx.maxAmmo = ShootingPlayerController::kMaxAmmo;
  ctx.machineGunAmmo = playerController_ ? playerController_->GetMachineGunAmmo() : 0;
  ctx.isMachineGun = playerController_ && (playerController_->GetCurrentWeapon() == ShootingPlayerController::WeaponType::MachineGun);
  ctx.reloadDuration = ShootingPlayerController::kReloadDuration;
  ctx.machineGunFireInterval = ShootingPlayerController::kMachineGunFireInterval;
  ctx.retryPos = gameOverOverlay_ ? gameOverOverlay_->GetRetryPos() : Vector2{300.0f, 360.0f};

  if (phase_ == Phase::Playing || phase_ == Phase::RestartSmoothing) {
    ctx.mode = ShootingDemoAI::BattleMode::Playing;
    if (waveManager_) {
      waveManager_->PopulateDemoAIContext(ctx, railManager_->GetProgress(), camera_->GetTranslate(), false);
    }
  } else if (phase_ == Phase::SwarmBattle) {
    ctx.mode = ShootingDemoAI::BattleMode::SwarmBattle;
    if (waveManager_) {
      waveManager_->PopulateDemoAIContext(ctx, railManager_->GetProgress(), camera_->GetTranslate(), true);
    }
  } else if (phase_ == Phase::GameOver) {
    ctx.mode = ShootingDemoAI::BattleMode::GameOver;
  } else {
    ctx.mode = ShootingDemoAI::BattleMode::Other;
  }

  demoAI_->Update(ctx);
  if (playerController_) {
    if (demoAI_->WantsSwitchToHandgun()) {
      playerController_->SetCurrentWeapon(ShootingPlayerController::WeaponType::Handgun);
    } else if (demoAI_->WantsSwitchToMachineGun()) {
      playerController_->SetCurrentWeapon(ShootingPlayerController::WeaponType::MachineGun);
    }
  }
}

void ShootingScene::StartBossIntro() {
  phase_ = Phase::BossIntro;
  phaseTimer_ = 0.0f;
  if (playerController_) {
    playerController_->ResetCover();
  }
  bossIntroStartYaw_ = cameraYaw_;
  bossIntroStartPitch_ = cameraBasePitch_;
  damageEffectStrength_ = 0.0f;
  radialBlurTimer_ = 0.0f;
  randomNoiseTimer_ = 0.0f;
  PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
  PostProcessManager::GetInstance()->SetStrength(1.0f);

  if (waveManager_) {
    Vector3 basePos = railManager_->GetCameraRailPosition(railManager_->GetMaxProgress(), playerController_->GetEyeHeight());
    float baseYaw = (levelData_ && !levelData_->players.empty()) ? levelData_->players[0].rotation.y : 0.0f;
    waveManager_->StartBossIntro(basePos, baseYaw);
  }
}
