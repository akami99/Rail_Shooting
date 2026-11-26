#include "ShootingScene.h"
#include "ApplicationConfig.h"
#include "BlendMode.h"
#include "GameSession.h"
#include "MathUtils.h"
#include "ParticleManager.h"
#include "PostProcessManager.h"
#include "SceneManager.h"
#include "Win32Window.h"
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#ifdef USE_IMGUI
#include "imgui.h"
#endif // USE_IMGUI

using namespace MathUtils;
using namespace BlendMode;

#ifdef USE_IMGUI
// ImGui操作の更新
void ShootingScene::UpdateImGui() {
  // メイン設定ウィンドウの位置とサイズ
  ImGui::SetNextWindowPos(ImVec2(Win32Window::kClientWidth - 10.0f, 10.0f),
                          ImGuiCond_Once, ImVec2(1.0f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2(450.0f, 600.0f), ImGuiCond_Once);

  if (ImGui::Begin("Settings")) {
    UpdateImGui_GlobalSettings();
    UpdateImGui_GameCamera();
    UpdateImGui_IntroSettings();
    UpdateImGui_Object3d();
    UpdateImGui_Skybox();
    UpdateImGui_Particle();
  }
  ImGui::End();

  if (effectEditor_) {
    effectEditor_->UpdateImGui(camera_.get());
  }

  UpdateImGui_GameStatus();
}

void ShootingScene::UpdateImGui_Particle() {
  ImGui::Separator();
  if (ImGui::TreeNode("Particle / Effect Tool")) {
    if (effectEditor_) {
      bool isOpen = effectEditor_->IsWindowOpen();
      if (ImGui::Checkbox("Open Effect Editor Window", &isOpen)) {
        effectEditor_->SetWindowOpen(isOpen);
      }
    }
    ImGui::TreePop();
  }
}

// ImGuiでグローバル設定のパラメータを調整するための関数
void ShootingScene::UpdateImGui_GlobalSettings() {
  ImGui::Separator();
  if (ImGui::TreeNode("Global Settings")) {
    ImGui::Separator();
    ImGui::Text("Blend Mode");
    const char *blendModeNames[] = {"None",        "Normal",   "Add",
                                    "Subtractive", "Multiply", "Screen"};
    int currentBlendModeInt = static_cast<int>(currentBlendMode_);
    if (ImGui::Combo("##blendMode", &currentBlendModeInt, blendModeNames,
                     IM_ARRAYSIZE(blendModeNames))) {
      currentBlendMode_ = static_cast<BlendState>(currentBlendModeInt);
    }
    ImGui::Separator();
    // ヒットポイント・弾薬・無敵パラメータ
    if (playerController_) {
      playerController_->UpdateGlobalDebugImGui();
    }
    ImGui::Checkbox("Debug Pause", &isDebugPaused_);
    ImGui::Separator();

    if (!sectionProgresses_.empty()) {
      int selIdx = currentSectionIndex_;
      std::vector<std::string> comboItems;
      for (size_t i = 0; i < sectionProgresses_.size(); ++i) {
        if (i == 0) {
          comboItems.push_back("0: Start (0.0)");
        } else {
          comboItems.push_back(std::to_string(i) + ": Battle (" +
                               std::to_string(static_cast<int>(sectionProgresses_[i])) +
                               ")");
        }
      }

      std::vector<const char *> comboChars;
      for (const auto &item : comboItems) {
        comboChars.push_back(item.c_str());
      }

      if (ImGui::Combo("Jump to Phase/Section", &selIdx, comboChars.data(),
                       static_cast<int>(comboChars.size()))) {
        JumpToSection(selIdx);
      }
      if (ImGui::Button("Jump to Swarm Boss Battle")) {
        isAutoClearing_ = false;
        autoClearAttackTimer_ = 0.0f;
        if (transitionDirector_) { transitionDirector_->ClearPreviewMode(); }
        if (hud_) { hud_->HideReadyGo(); }
        if (railManager_ && transitionDirector_) {
          railManager_->ClearTerrainDissolve(transitionDirector_->GetTransitionEdgeColor());
        }
        float baseYaw = (levelData_ && !levelData_->players.empty()) ? levelData_->players[0].rotation.y : 0.0f;
        Vector3 basePos = railManager_->GetCameraRailPosition(railManager_->GetMaxProgress(), playerController_->GetEyeHeight());
        camera_->SetTranslate(basePos);
        railManager_->SetProgress(railManager_->GetMaxProgress());
        railManager_->SetMovementPaused(false);
        phase_ = Phase::SwarmBattle;
        if (waveManager_) {
          waveManager_->JumpToBossBattle(basePos, baseYaw);
        }
        PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
      }
      ImGui::Separator();
    }

    // --- シーンを初期化（論理リセット） ---
    if (ImGui::Button("Reset Game")) {
      isAutoClearing_ = false;
      autoClearAttackTimer_ = 0.0f;
      if (playerController_) {
        playerController_->ResetState(false);
      }
      orbitAngle_ = 0.0f;
      targetTimer_ = 0.0f;
      cameraYaw_ = 0.0f;
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
      if (waveManager_) {
        waveManager_->ResetForGameReset(mainViewIndex_, camera_.get());
      }

      if (playerController_) {
        playerController_->ResetAmmoUnitsLayout();
        if (hud_) {
          hud_->ResetLifeGauge(playerController_->GetHp(), ShootingPlayerController::kMaxHp);
        }
      }

      // カメラを初期位置に戻す
      railManager_->SetProgress(0.0f);
      cameraShakeTimer_ = 0.0f;
      railManager_->SetMovementPaused(false);
      Vector3 startCamPos = railManager_->GetCameraRailPosition(railManager_->GetProgress(), playerController_->GetEyeHeight());
      camera_->SetTranslate(startCamPos);
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
      cameraBasePitch_ = baseCombatPitch_;
      cameraYaw_ = baseCombatYaw_;
      camera_->SetRotate({cameraBasePitch_, cameraYaw_, cameraBaseRoll_});

      // 残留パーティクルを完全にクリア
      if (waveManager_) {
        waveManager_->ClearSwarmDrones();
      }
      ParticleManager::GetInstance()->ClearAllParticles();
      // スムージングを最大限（31）適用してリスタート
      if (transitionDirector_) {
        transitionDirector_->BeginRestartSmoothing();
      }
      phase_ = Phase::RestartSmoothing;
      phaseTimer_ = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Go to Clear Scene")) {
      GameSession::GetInstance().SetClearResult(score_, gameTimer_);
      SceneManager::GetInstance()->ChangeScene("CLEAR");
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Game")) {
      StartAutoClear();
    }
    ImGui::TreePop();
  }
}

// ImGuiでゲームカメラのパラメータを調整するための関数
void ShootingScene::UpdateImGui_GameCamera() {
  ImGui::Separator();
  if (ImGui::TreeNode("Game Camera")) {
    Vector3 camPos = camera_->GetTranslate();
    Vector3 camRot = camera_->GetRotate();
    if (ImGui::DragFloat3("Position", &camPos.x, 0.1f)) {
      camera_->SetTranslate(camPos);
    }
    if (ImGui::DragFloat3("Rotation", &camRot.x, 0.1f)) {
      camera_->SetRotate(camRot);
      cameraYaw_ = camRot.y;
    }
    if (ImGui::DragFloat("FOV", &cameraFovY_, 0.1f, 10.0f, 120.0f)) {
      camera_->SetFovY(cameraFovY_ * (std::numbers::pi_v<float> / 180.0f));
    }
    ImGui::Separator();
    if (playerController_) {
      playerController_->UpdateCoverImGui(isAutoClearing_);
    }
    ImGui::Separator();
    ImGui::Text("Rail Following & Combat Orientation");
    ImGui::DragFloat("Base Combat Yaw", &baseCombatYaw_, 0.02f, -3.14f, 3.14f, "%.2f rad");
    ImGui::DragFloat("Base Combat Pitch", &baseCombatPitch_, 0.02f, -1.5f, 1.5f, "%.2f rad");
    ImGui::DragFloat("Look Ahead Dist", &lookAheadDist_, 0.1f, 0.2f, 5.0f, "%.1f m");
    ImGui::DragFloat("Transition Dist", &battleTransitionDistance_, 0.2f, 1.0f, 15.0f, "%.1f m");
    ImGui::DragFloat("Camera Rotate Speed", &cameraRotateSpeed_, 0.01f, 0.01f, 0.3f, "%.2f");
    if (ImGui::TreeNode("Section Follow Direction Flags")) {
      for (size_t i = 0; i < sectionLookDirectionFlags_.size(); ++i) {
        bool flag = sectionLookDirectionFlags_[i];
        std::string label = "Sec " + std::to_string(i) + " (" +
                            std::to_string(static_cast<int>(sectionProgresses_[i])) + "m - " +
                            ((i + 1 < sectionProgresses_.size()) ? std::to_string(static_cast<int>(sectionProgresses_[i + 1])) + "m" : "End") + ")";
        if (ImGui::Checkbox(label.c_str(), &flag)) {
          sectionLookDirectionFlags_[i] = flag;
        }
      }
      ImGui::TreePop();
    }
    ImGui::Separator();
    ImGui::Text("Camera Shake");
    ImGui::SliderFloat("Shake Intensity", &cameraShakeIntensity_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Shake Duration", &cameraShakeDuration_, 0.1f, 1.0f, "%.2f s");
    if (ImGui::Button("Trigger Normal Shake")) {
      cameraShakeIntensity_ = 0.20f;
      cameraShakeDuration_ = 0.25f;
      cameraShakeTimer_ = 0.25f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Trigger Blast Shake")) {
      cameraShakeIntensity_ = 0.45f;
      cameraShakeDuration_ = 0.45f;
      cameraShakeTimer_ = 0.45f;
    }
    ImGui::TreePop();
  }
}

// ImGuiでObject3dのパラメータを調整するための関数

// ImGuiでスタート演出・終点スウォームアンカーのパラメータを調整するための関数
void ShootingScene::UpdateImGui_IntroSettings() {
  if (!transitionDirector_) return;
  auto actions = transitionDirector_->UpdateIntroImGui(railManager_.get(), waveManager_.get(), hud_.get());
  if (actions.replayIntro) {
    phase_ = Phase::Intro;
    transitionDirector_->StartIntroFastForward();
    phaseTimer_ = 0.0f;
    SceneManager::GetInstance()->ChangeScene("SHOOTING");
  }
  if (actions.playReadyGoOnly) {
    phase_ = Phase::Intro;
    transitionDirector_->StartIntroReadyGo();
    phaseTimer_ = 0.0f;
  }
}



void ShootingScene::UpdateImGui_Object3d() {
  ImGui::Separator();
  // エネミーのパラメータを調整
  ImGui::Text("Movement Paused: %s", railManager_->IsMovementPaused() ? "True" : "False");
  ImGui::Text("Progress: %.2f / %.2f", railManager_->GetProgress(), railManager_->GetMaxProgress());
  if (waveManager_) {
    bool reqSwarm = false;
    waveManager_->UpdateObject3dImGui(camera_.get(), reqSwarm);
    if (reqSwarm) {
      phase_ = Phase::SwarmBattle;
    }
  }
  ImGui::Separator();
  // 地形オブジェクトのパラメータを調整
  if (railManager_) {
    railManager_->UpdateImGui();
  }
}

// ImGuiでSkyboxのパラメータを調整するための関数
void ShootingScene::UpdateImGui_Skybox() {
  ImGui::Separator();
  if (ImGui::TreeNode("Skybox")) {
    ImGui::Checkbox("Show Skybox", &isShowSkybox_);
    if (skybox_) {
      Vector3 skyboxScale = skybox_->GetScale();
      Vector3 skyboxRotation = skybox_->GetRotate();
      Vector3 skyboxPosition = skybox_->GetTranslate();
      ImGui::DragFloat3("scale", &skyboxScale.x, 0.1f);
      ImGui::DragFloat3("rotate", &skyboxRotation.x, 0.01f);
      ImGui::DragFloat3("translate", &skyboxPosition.x, 0.1f);
      skybox_->SetScale(skyboxScale);
      skybox_->SetRotate(skyboxRotation);
      skybox_->SetTranslate(skyboxPosition);

      // シェーダーモード切り替え (CubeMap / Tile)
      const char* modes[] = { "CubeMap (skybox.dds)", "Tile (2D Tiling)" };
      int currentMode = (skybox_->GetMode() == Skybox::SkyboxMode::Tile) ? 1 : 0;
      if (ImGui::Combo("Shader Mode", &currentMode, modes, IM_ARRAYSIZE(modes))) {
        if (currentMode == 0) {
          skybox_->SetCubeMap("skybox.dds");
        } else {
          skybox_->SetTileTexture(skyboxTilePath_);
        }
      }

      // タイル描画モード時のパラメータ調整
      if (skybox_->GetMode() == Skybox::SkyboxMode::Tile) {
        // タイル画像のタイリング調整 (UV Scale)
        if (ImGui::DragFloat2("UV Scale", &skyboxUvScale_.x, 0.1f, 0.1f, 500.0f)) {
          skybox_->SetUvScale(skyboxUvScale_);
        }

        // タイルテクスチャの選択
        const char* tileTextures[] = {
          "tile.png",
          "checkerBoard.png",
          "uvChecker.png",
          "floor.png",
          "grass.png",
          "white.png"
        };
        static int currentTextureIdx = 0;
        if (ImGui::Combo("Tile Texture", &currentTextureIdx, tileTextures, IM_ARRAYSIZE(tileTextures))) {
          skyboxTilePath_ = tileTextures[currentTextureIdx];
          skybox_->SetTileTexture(skyboxTilePath_);
        }
      }
    }
    ImGui::TreePop();
  }
}

// ImGuiでゲームの進行状況やプレイヤーの状態を表示するための関数
void ShootingScene::UpdateImGui_GameStatus() {
  ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Once);
  ImGui::SetNextWindowSize(ImVec2(200.0f, 100.0f), ImGuiCond_Once);
  ImGui::Begin("Game Status", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
  if (phase_ == Phase::GameOver) {
    if (playerController_ && playerController_->GetHp() <= 0) {
      ImGui::Text("GAME OVER");
    } else {
      ImGui::Text("CLEAR!");
    }
  } else if (phase_ == Phase::RestartSmoothing) {
    ImGui::Text("Restarting...");
  } else if (phase_ == Phase::Intro && transitionDirector_ && transitionDirector_->IsIntroConstructActive()) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "CONSTRUCTING STAGE... (%.1f s)", phaseTimer_);
    if (ImGui::Button("Skip Intro")) {
      phaseTimer_ = ShootingTransitionDirector::kIntroConstructDuration;
    }
  } else if (phase_ == Phase::SwarmBattle) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.2f, 1.0f), "SWARM BATTLE! (%.1f s)", waveManager_ ? waveManager_->GetSwarmBattleTimer() : 0.0f);
  } else {
    ImGui::ProgressBar(railManager_->GetProgress() / railManager_->GetMaxProgress(), ImVec2(0.0f, 0.0f),
                       "Progress");
  }
  ImGui::Separator();
  ImGui::Text("Player Status");
  ImGui::Separator();
  if (playerController_) {
    playerController_->UpdateStatusImGui();
  }
  ImGui::Separator();
  ImGui::Text("Control");
  ImGui::Separator();
  ImGui::Text("Mouse: Aim");
  ImGui::Text("LMB: Shoot (Handgun: 0.5s Single / MachineGun: 0.3s Auto)");
  ImGui::Text("RMB: Switch Weapon (Handgun <-> MachineGun)");
  ImGui::Text("SPACE: Cover (Reload Handgun)");
  ImGui::Text("ESC: Return to Title");
  ImGui::Text("R: Reset Scene");

  if (hud_) {
    hud_->UpdateGuidesImGui();
  }

  if (playerController_) {
    playerController_->UpdateWeaponLayoutImGui();
  }

  if (hud_) {
    hud_->UpdateProgressBarImGui();
    hud_->UpdateTimerImGui(gameOverTimer_);
  }

  if (gameOverOverlay_) {
    gameOverOverlay_->UpdateImGui();
  }

  ImGui::End();
}

void ShootingScene::UpdateImGui_SwarmCollisionDebug() {
  ImGui::SetNextWindowPos(ImVec2(240.0f, 10.0f), ImGuiCond_Once);
  ImGui::SetNextWindowSize(ImVec2(380.0f, 360.0f), ImGuiCond_Once);
  if (ImGui::Begin("Swarm Collision Debug (O: Exit)")) {
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "Mode Active: Game Paused (Press 'O' to resume)");
    ImGui::Separator();

    ImGui::SliderFloat("Preview Distance", &debugCollisionDist_, 1.5f, 15.0f, "%.2f m");
    ImGui::Checkbox("Auto Rotate", &debugAutoRotate_);
    if (!debugAutoRotate_) {
      ImGui::SliderFloat("Rotation Y", &debugDroneRotY_, -3.14159f, 3.14159f, "%.2f rad");
    } else {
      debugDroneRotY_ += ApplicationConfig::kDeltaTime * 0.8f;
      if (debugDroneRotY_ > 3.14159f) {
        debugDroneRotY_ -= 6.28318f;
      }
    }
    ImGui::Checkbox("Show 2nd Drone (Contact Check)", &debugShowSecondDrone_);
    ImGui::Separator();

    float droneScale = 0.32f;
    float colRadius = 0.32f;
    if (waveManager_ && waveManager_->UpdateSwarmCollisionDebugImGui(droneScale, colRadius)) {

      // カメラ前方にドローンとコリジョン球ワイヤーを配置して行列更新
      Vector3 camPos = camera_->GetTranslate();
      Vector3 camRot = camera_->GetRotate();
      Vector3 forward = {
        std::cos(camRot.x) * std::sin(camRot.y),
        -std::sin(camRot.x),
        std::cos(camRot.x) * std::cos(camRot.y)
      };
      Vector3 right = {
        std::cos(camRot.y),
        0.0f,
        -std::sin(camRot.y)
      };
      Vector3 centerPos = Add(camPos, Multiply(debugCollisionDist_, forward));

      Vector3 pos1 = debugShowSecondDrone_ ? Subtract(centerPos, Multiply(colRadius, right)) : centerPos;
      Vector3 pos2 = Add(centerPos, Multiply(colRadius, right));

      if (debugDroneObj_) {
        debugDroneObj_->SetTranslate(pos1);
        debugDroneObj_->SetRotation({ 0.0f, debugDroneRotY_, 0.0f });
        debugDroneObj_->SetScale({ droneScale, droneScale, droneScale });
        debugDroneObj_->Update(mainViewIndex_, camera_.get());
      }
      if (debugCollisionSphereObj_) {
        debugCollisionSphereObj_->SetTranslate(pos1);
        debugCollisionSphereObj_->SetRotation({ 0.0f, 0.0f, 0.0f });
        debugCollisionSphereObj_->SetScale({ colRadius, colRadius, colRadius });
        debugCollisionSphereObj_->Update(mainViewIndex_, camera_.get());
      }
      if (debugShowSecondDrone_) {
        if (debugDroneObj2_) {
          debugDroneObj2_->SetTranslate(pos2);
          debugDroneObj2_->SetRotation({ 0.0f, -debugDroneRotY_, 0.0f });
          debugDroneObj2_->SetScale({ droneScale, droneScale, droneScale });
          debugDroneObj2_->Update(mainViewIndex_, camera_.get());
        }
        if (debugCollisionSphereObj2_) {
          debugCollisionSphereObj2_->SetTranslate(pos2);
          debugCollisionSphereObj2_->SetRotation({ 0.0f, 0.0f, 0.0f });
          debugCollisionSphereObj2_->SetScale({ colRadius, colRadius, colRadius });
          debugCollisionSphereObj2_->Update(mainViewIndex_, camera_.get());
        }
      }
    }
  }
  ImGui::End();
}
#endif // USE_IMGUI
