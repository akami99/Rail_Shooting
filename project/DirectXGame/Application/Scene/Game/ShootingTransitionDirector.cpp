#include "ShootingTransitionDirector.h"
#include "ApplicationConfig.h"
#include "BlendMode.h"
#include "Camera.h"
#include "Input.h"
#include "Object3dCommon.h"
#include "Particle/ParticleManager.h"
#include "PostProcessManager.h"
#include "ShootingHUD.h"
#include "ShootingPlayerController.h"
#include "ShootingWaveManager.h"
#include "Skybox.h"
#include "StageRailManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include "imgui.h"
#endif // USE_IMGUI

namespace {
constexpr float kHalfPI = std::numbers::pi_v<float> * 0.5f;
}

// =========================================================================
// 入場カットシーン State Pattern 実装
// =========================================================================
class ShootingTransitionDirector::IntroConstructState final : public ShootingTransitionDirector::IIntroState {
public:
  void Update(ShootingTransitionDirector& director, IntroContext& ctx) override;
  bool NeedsCrosshairUpdate() const override { return true; }
  bool IsConstructState() const override { return true; }
};

class ShootingTransitionDirector::IntroFastForwardState final : public ShootingTransitionDirector::IIntroState {
public:
  void Update(ShootingTransitionDirector& director, IntroContext& ctx) override;
};

class ShootingTransitionDirector::IntroSwarmShowcaseState final : public ShootingTransitionDirector::IIntroState {
public:
  void Update(ShootingTransitionDirector& director, IntroContext& ctx) override;
};

class ShootingTransitionDirector::IntroRewindState final : public ShootingTransitionDirector::IIntroState {
public:
  void Update(ShootingTransitionDirector& director, IntroContext& ctx) override;
  bool NeedsCrosshairUpdate() const override { return true; }
};

class ShootingTransitionDirector::IntroReadyGoState final : public ShootingTransitionDirector::IIntroState {
public:
  void Update(ShootingTransitionDirector& director, IntroContext& ctx) override;
  bool IsReadyGoState() const override { return true; }
};

void ShootingTransitionDirector::IntroConstructState::Update(ShootingTransitionDirector& director, IntroContext& ctx) {
  if (director.UpdateIntroConstruct(ctx.phaseTimer, ctx.railManager, ctx.camera, ctx.skybox, ctx.mainViewIndex)) {
    director.ChangeIntroState(std::make_unique<IntroFastForwardState>());
  }
}

void ShootingTransitionDirector::IntroFastForwardState::Update(ShootingTransitionDirector& director, IntroContext& ctx) {
  if (ctx.isPaused) return;
  if (director.UpdateIntroFastForward(ctx.phaseTimer, ctx.railManager, ctx.playerController,
                                      ctx.waveManager, ctx.camera, ctx.skybox, ctx.mainViewIndex)) {
    director.ChangeIntroState(std::make_unique<IntroSwarmShowcaseState>());
  }
}

void ShootingTransitionDirector::IntroSwarmShowcaseState::Update(ShootingTransitionDirector& director, IntroContext& ctx) {
  if (ctx.isPaused) return;
  if (director.UpdateIntroSwarmShowcase(ctx.phaseTimer, ctx.railManager, ctx.playerController,
                                        ctx.waveManager, ctx.camera, ctx.skybox,
                                        ctx.cameraBasePitch, ctx.cameraYaw, ctx.mainViewIndex)) {
    director.ChangeIntroState(std::make_unique<IntroRewindState>());
  }
}

void ShootingTransitionDirector::IntroRewindState::Update(ShootingTransitionDirector& director, IntroContext& ctx) {
  if (ctx.isPaused) return;
  if (director.UpdateIntroRewind(ctx.phaseTimer, ctx.railManager, ctx.playerController,
                                 ctx.waveManager, ctx.camera, ctx.skybox,
                                 ctx.cameraBasePitch, ctx.cameraYaw, ctx.cameraBaseRoll,
                                 ctx.baseCombatPitch, ctx.baseCombatYaw, ctx.mainViewIndex)) {
    director.ChangeIntroState(std::make_unique<IntroReadyGoState>());
  }
}

void ShootingTransitionDirector::IntroReadyGoState::Update(ShootingTransitionDirector& director, IntroContext& ctx) {
  if (ctx.isPaused) return;
  if (director.UpdateIntroReadyGo(ctx.phaseTimer, ctx.hud, ctx.railManager,
                                  ctx.camera, ctx.skybox, ctx.mainViewIndex)) {
    director.ChangeIntroState(nullptr);
  }
}

void ShootingTransitionDirector::ChangeIntroState(std::unique_ptr<IIntroState> nextState) {
  introState_ = std::move(nextState);
}

void ShootingTransitionDirector::StartIntroConstruct() {
  ChangeIntroState(std::make_unique<IntroConstructState>());
}

void ShootingTransitionDirector::StartIntroFastForward() {
  ChangeIntroState(std::make_unique<IntroFastForwardState>());
}

void ShootingTransitionDirector::StartIntroReadyGo() {
  ChangeIntroState(std::make_unique<IntroReadyGoState>());
}

bool ShootingTransitionDirector::UpdateIntro(IntroContext& ctx) {
  if (!introState_) {
    return true;
  }
  introState_->Update(*this, ctx);
  return introState_ == nullptr;
}

bool ShootingTransitionDirector::NeedsCrosshairUpdateInIntro() const {
  return introState_ && introState_->NeedsCrosshairUpdate();
}

bool ShootingTransitionDirector::IsIntroConstructActive() const {
  return introState_ && introState_->IsConstructState();
}

bool ShootingTransitionDirector::IsIntroReadyGoActive() const {
  return introState_ && introState_->IsReadyGoState();
}

void ShootingTransitionDirector::InitializeBlackout() {
  vignetteScale_ = 0.0f;
  vignetteExponent_ = 5.0f;
  PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeVignette);
  PostProcessManager::GetInstance()->SetVignetteParams(vignetteScale_, vignetteExponent_);
}

void ShootingTransitionDirector::BeginClearVignette() {
  vignetteScale_ = 16.0f;
  vignetteExponent_ = 0.8f;
  PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeVignette);
  PostProcessManager::GetInstance()->SetVignetteParams(vignetteScale_, vignetteExponent_);
}

void ShootingTransitionDirector::BeginReturnToTitleVignette() {
  vignetteScale_ = 16.0f;
  vignetteExponent_ = 0.8f;
  PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeVignette);
  PostProcessManager::GetInstance()->SetStrength(1.0f);
  PostProcessManager::GetInstance()->SetVignetteParams(vignetteScale_, vignetteExponent_);
}

void ShootingTransitionDirector::BeginRestartSmoothing() {
  smoothingKernel_ = 31.0f;
  PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeSmoothing);
  PostProcessManager::GetInstance()->SetSmoothingParams(static_cast<int>(smoothingKernel_));
}

bool ShootingTransitionDirector::UpdateIntroConstruct(float& phaseTimer, StageRailManager* railManager,
                                                      Camera* camera, Skybox* skybox, int mainViewIndex) {
  phaseTimer += ApplicationConfig::kDeltaTime;

  if (Input::GetInstance()->IsKeyTriggered(DIK_SPACE) ||
      Input::GetInstance()->IsKeyTriggered(DIK_RETURN)) {
    phaseTimer = kIntroConstructDuration;
  }

  bool finished = false;
  if (phaseTimer >= kIntroConstructDuration) {
    phaseTimer = 0.0f;
    PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
    if (railManager) {
      railManager->ClearTerrainDissolve(kTransitionEdgeColor);
    }
    finished = true;
  } else {
    if (phaseTimer < kBlinkDuration) {
      float t = phaseTimer / kBlinkDuration;
      float openness = 0.0f;
      if (t < 0.35f) {
        float p = t / 0.35f;
        openness = 0.45f * std::sin(p * kHalfPI);
      } else if (t < 0.55f) {
        float p = (t - 0.35f) / 0.20f;
        openness = 0.45f + (0.15f - 0.45f) * std::sin(p * kHalfPI);
      } else {
        float p = (t - 0.55f) / 0.45f;
        float eased = 1.0f - (1.0f - p) * (1.0f - p);
        openness = 0.15f + (1.0f - 0.15f) * eased;
      }

      float scale = 16.0f * openness;
      float exponent = 5.0f + (0.8f - 5.0f) * openness;
      PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeVignette);
      PostProcessManager::GetInstance()->SetVignetteParams(scale, exponent);
    } else {
      PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
    }
  }

  if (camera) camera->Update();
  if (railManager) railManager->UpdateTerrain(mainViewIndex, camera);
  if (skybox) skybox->Update(mainViewIndex, camera);

  return finished;
}

bool ShootingTransitionDirector::UpdatePreviewMode(StageRailManager* railManager, ShootingPlayerController* playerController,
                                                   ShootingWaveManager* waveManager, Camera* camera, Skybox* skybox,
                                                   float cameraBasePitch, float cameraYaw, int mainViewIndex) {
  if (!isIntroPreviewMode_ || !railManager || !camera) return false;

  railManager->SetProgress(railManager->GetMaxProgress() - introCameraStopOffset_);
  float eyeH = playerController ? playerController->GetEyeHeight() : 1.4f;
  camera->SetTranslate(railManager->GetCameraRailPosition(railManager->GetProgress(), eyeH));
  camera->SetRotate({ cameraBasePitch + introCameraPitch_, cameraYaw + introCameraYaw_, 0.0f });
  camera->Update();
  railManager->UpdateTerrain(mainViewIndex, camera);
  if (skybox) skybox->Update(mainViewIndex, camera);

  Vector3 endRailPos = railManager->CalculateRailPosition(railManager->GetMaxProgress());
  Vector3 swarmCenter{ endRailPos.x + introSwarmOffset_.x, endRailPos.y + introSwarmOffset_.y, endRailPos.z + introSwarmOffset_.z };
  if (waveManager) {
    waveManager->UpdateIntroSwarm(ApplicationConfig::kDeltaTime, swarmCenter, camera);
  }
  PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
  return true;
}

bool ShootingTransitionDirector::UpdateIntroFastForward(float& phaseTimer, StageRailManager* railManager,
                                                        ShootingPlayerController* playerController, ShootingWaveManager* waveManager,
                                                        Camera* camera, Skybox* skybox, int mainViewIndex) {
  phaseTimer += ApplicationConfig::kDeltaTime;

  if (Input::GetInstance()->IsKeyTriggered(DIK_SPACE) ||
      Input::GetInstance()->IsKeyTriggered(DIK_RETURN)) {
    float remainingTime = kFastForwardDuration - phaseTimer;
    int catchUpSteps = static_cast<int>(remainingTime / ApplicationConfig::kDeltaTime);

    Vector3 endRailPos = railManager->CalculateRailPosition(railManager->GetMaxProgress());
    Vector3 swarmCenter{ endRailPos.x + introSwarmOffset_.x, endRailPos.y + introSwarmOffset_.y, endRailPos.z + introSwarmOffset_.z };
    if (waveManager) {
      for (int i = 0; i < catchUpSteps; ++i) {
        waveManager->UpdateIntroSwarm(ApplicationConfig::kDeltaTime, swarmCenter, camera);
      }
    }
    phaseTimer = kFastForwardDuration;
  }

  bool finished = false;
  if (phaseTimer >= kFastForwardDuration) {
    phaseTimer = 0.0f;
    PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
    finished = true;
  } else {
    float t = phaseTimer / kFastForwardDuration;
    float ease = t * t;
    railManager->SetProgress(ease * railManager->GetMaxProgress());
    float eyeH = playerController ? playerController->GetEyeHeight() : 1.4f;
    camera->SetTranslate(railManager->GetCameraRailPosition(railManager->GetProgress(), eyeH));

    PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeRadialBlur);
    float blurWidth = 0.04f * (1.0f - std::abs(t - 0.5f) * 2.0f);
    PostProcessManager::GetInstance()->SetRadialBlurParams(0.5f, 0.5f, blurWidth, 12);
  }

  camera->Update();
  railManager->UpdateTerrain(mainViewIndex, camera);
  if (skybox) skybox->Update(mainViewIndex, camera);

  Vector3 endRailPos = railManager->CalculateRailPosition(railManager->GetMaxProgress());
  Vector3 swarmCenter{ endRailPos.x + introSwarmOffset_.x, endRailPos.y + introSwarmOffset_.y, endRailPos.z + introSwarmOffset_.z };
  if (waveManager) {
    waveManager->UpdateIntroSwarm(ApplicationConfig::kDeltaTime, swarmCenter, camera);
  }

  return finished;
}

bool ShootingTransitionDirector::UpdateIntroSwarmShowcase(float& phaseTimer, StageRailManager* railManager,
                                                          ShootingPlayerController* playerController, ShootingWaveManager* waveManager,
                                                          Camera* camera, Skybox* skybox,
                                                          float cameraBasePitch, float cameraYaw, int mainViewIndex) {
  phaseTimer += ApplicationConfig::kDeltaTime;

  if (Input::GetInstance()->IsKeyTriggered(DIK_SPACE) ||
      Input::GetInstance()->IsKeyTriggered(DIK_RETURN)) {
    phaseTimer = kSwarmShowcaseDuration;
  }

  bool finished = false;
  if (phaseTimer >= kSwarmShowcaseDuration) {
    phaseTimer = 0.0f;
    finished = true;
  } else {
    float t = phaseTimer / kSwarmShowcaseDuration;
    float weight = 0.0f;
    if (t < 0.30f) {
      float p = t / 0.30f;
      weight = std::sin(p * kHalfPI);
    } else if (t < 0.70f) {
      weight = 1.0f;
    } else {
      float p = (t - 0.70f) / 0.30f;
      weight = std::cos(p * kHalfPI);
    }
    railManager->SetProgress(railManager->GetMaxProgress() - introCameraStopOffset_);
    float eyeH = playerController ? playerController->GetEyeHeight() : 1.4f;
    camera->SetTranslate(railManager->GetCameraRailPosition(railManager->GetProgress(), eyeH));
    camera->SetRotate({
        cameraBasePitch + weight * introCameraPitch_,
        cameraYaw + weight * introCameraYaw_,
        0.0f
    });
  }

  camera->Update();
  railManager->UpdateTerrain(mainViewIndex, camera);
  if (skybox) skybox->Update(mainViewIndex, camera);

  Vector3 endRailPos = railManager->CalculateRailPosition(railManager->GetMaxProgress());
  introSwarmOffset_.x = -200.0f;
  introSwarmOffset_.z = 400.0f;
  Vector3 swarmCenter{ endRailPos.x + introSwarmOffset_.x, endRailPos.y + introSwarmOffset_.y, endRailPos.z + introSwarmOffset_.z };
  if (waveManager) {
    waveManager->UpdateIntroSwarm(ApplicationConfig::kDeltaTime, swarmCenter, camera);
  }

  return finished;
}

bool ShootingTransitionDirector::UpdateIntroRewind(float& phaseTimer, StageRailManager* railManager,
                                                   ShootingPlayerController* playerController, ShootingWaveManager* waveManager,
                                                   Camera* camera, Skybox* skybox,
                                                   float& cameraBasePitch, float& cameraYaw, float cameraBaseRoll,
                                                   float baseCombatPitch, float baseCombatYaw, int mainViewIndex) {
  phaseTimer += ApplicationConfig::kDeltaTime;

  if (Input::GetInstance()->IsKeyTriggered(DIK_SPACE) ||
      Input::GetInstance()->IsKeyTriggered(DIK_RETURN)) {
    phaseTimer = kRewindDuration;
  }

  bool finished = false;
  float eyeH = playerController ? playerController->GetEyeHeight() : 1.4f;
  if (phaseTimer >= kRewindDuration) {
    phaseTimer = 0.0f;
    railManager->SetProgress(0.0f);
    camera->SetTranslate(railManager->GetCameraRailPosition(0.0f, eyeH));
    cameraBasePitch = baseCombatPitch;
    cameraYaw = baseCombatYaw;
    camera->SetRotate({ cameraBasePitch, cameraYaw, cameraBaseRoll });
    if (waveManager) {
      waveManager->ClearSwarmDrones();
    }
    PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
    finished = true;
  } else {
    if (waveManager && phaseTimer >= kRewindDuration / 2.0f) {
      waveManager->ClearSwarmDrones();
    }

    float t = phaseTimer / kRewindDuration;
    float ease = 1.0f - (t * t);
    railManager->SetProgress(ease * railManager->GetMaxProgress());
    camera->SetTranslate(railManager->GetCameraRailPosition(railManager->GetProgress(), eyeH));

    PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeRadialBlur);
    float blurWidth = 0.04f * (1.0f - std::abs(t - 0.5f) * 2.0f);
    PostProcessManager::GetInstance()->SetRadialBlurParams(0.5f, 0.5f, blurWidth, 12);
  }

  camera->Update();
  railManager->UpdateTerrain(mainViewIndex, camera);
  if (skybox) skybox->Update(mainViewIndex, camera);

  Vector3 endRailPos = railManager->CalculateRailPosition(railManager->GetMaxProgress());
  Vector3 swarmCenter{ endRailPos.x + introSwarmOffset_.x, endRailPos.y + introSwarmOffset_.y, endRailPos.z + introSwarmOffset_.z };
  if (waveManager) {
    waveManager->UpdateIntroSwarm(ApplicationConfig::kDeltaTime, swarmCenter, camera);
  }

  return finished;
}

bool ShootingTransitionDirector::UpdateIntroReadyGo(float& phaseTimer, ShootingHUD* hud, StageRailManager* railManager,
                                                    Camera* camera, Skybox* skybox, int mainViewIndex) {
  phaseTimer += ApplicationConfig::kDeltaTime;

  if (Input::GetInstance()->IsKeyTriggered(DIK_SPACE) ||
      Input::GetInstance()->IsKeyTriggered(DIK_RETURN)) {
    phaseTimer = ShootingHUD::kReadyGoTotalDuration;
  }

  bool finished = false;
  if (phaseTimer >= ShootingHUD::kReadyGoTotalDuration) {
    phaseTimer = 0.0f;
    if (hud) {
      hud->HideReadyGo();
    }
    finished = true;
  } else if (hud) {
    hud->UpdateReadyGo(phaseTimer);
  }

  if (camera) camera->Update();
  if (railManager) railManager->UpdateTerrain(mainViewIndex, camera);
  if (skybox) skybox->Update(mainViewIndex, camera);

  return finished;
}

bool ShootingTransitionDirector::UpdateReturnToTitleVignette(float& phaseTimer, StageRailManager* railManager,
                                                             Camera* camera, Skybox* skybox, int mainViewIndex) {
  phaseTimer += ApplicationConfig::kDeltaTime;
  float t = std::clamp(phaseTimer / kReturnToTitleDuration, 0.0f, 1.0f);

  float closing = t * t;
  float openness = 1.0f - closing;

  float scale = 16.0f * openness;
  float exponent = 0.8f + (5.0f - 0.8f) * closing;

  PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeVignette);
  PostProcessManager::GetInstance()->SetVignetteParams(scale, exponent);

  if (camera) camera->Update();
  if (railManager) railManager->UpdateTerrain(mainViewIndex, camera);
  if (skybox) skybox->Update(mainViewIndex, camera);

  return phaseTimer >= kReturnToTitleDuration;
}

bool ShootingTransitionDirector::UpdateClearVignette(float& phaseTimer, StageRailManager* railManager,
                                                     Camera* camera, Skybox* skybox, int mainViewIndex) {
  phaseTimer += ApplicationConfig::kDeltaTime;

  float openness = 1.0f;
  if (phaseTimer < 0.9f) {
    float bt = phaseTimer / 0.9f;
    if (bt < 0.45f) {
      float p = bt / 0.45f;
      openness = 1.0f - (1.0f - 0.08f) * (p * p);
    } else {
      float p = (bt - 0.45f) / 0.55f;
      float eased = 1.0f - (1.0f - p) * (1.0f - p);
      openness = 0.08f + (1.0f - 0.08f) * eased;
    }
  } else if (phaseTimer >= 1.4f) {
    float vt = std::clamp((phaseTimer - 1.4f) / (kClearTransitionDuration - 1.4f), 0.0f, 1.0f);
    openness = 1.0f - (vt * vt);
  }

  vignetteScale_ = 16.0f * openness;
  vignetteExponent_ = 0.8f + (5.0f - 0.8f) * (1.0f - openness);
  PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeVignette);
  PostProcessManager::GetInstance()->SetVignetteParams(vignetteScale_, vignetteExponent_);

  if (phaseTimer >= kClearTransitionDuration) {
    PostProcessManager::GetInstance()->SetVignetteParams(0.0f, 5.0f);
    return true;
  }

  if (camera) camera->Update();
  if (railManager) railManager->UpdateTerrain(mainViewIndex, camera);
  if (skybox) skybox->Update(mainViewIndex, camera);
  ParticleManager::GetInstance()->SetIsUpdate(true);
  ParticleManager::GetInstance()->SetUseBillboard(false);
  if (camera) {
    ParticleManager::GetInstance()->Update(*camera, ApplicationConfig::kDeltaTime);
  }
  return false;
}

bool ShootingTransitionDirector::UpdateRestartSmoothing(float& phaseTimer) {
  phaseTimer += ApplicationConfig::kDeltaTime;
  float t = std::clamp(phaseTimer / kSmoothingOutDuration, 0.0f, 1.0f);

  smoothingKernel_ = 31.0f + (1.0f - 31.0f) * t;
  int kernel = static_cast<int>(smoothingKernel_);
  if (kernel % 2 == 0) kernel++;
  if (kernel < 1) kernel = 1;

  PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeSmoothing);
  PostProcessManager::GetInstance()->SetSmoothingParams(kernel);

  if (phaseTimer >= kSmoothingOutDuration) {
    PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
    return true;
  }
  return false;
}

void ShootingTransitionDirector::DrawObjectWithIntroTransition(Object3d* obj, float startTime, float duration,
                                                               bool isIntroConstruct, float phaseTimer, int blendMode) const {
  if (!obj) return;

  if (!isIntroConstruct || phaseTimer >= startTime + duration) {
    obj->SetDissolveParams(0, 0.0f, 0.05f, kTransitionEdgeColor);
    obj->Draw(0);
    return;
  }

  if (phaseTimer <= startTime) {
    return;
  }

  float p = (phaseTimer - startTime) / duration;
  auto blend = static_cast<BlendMode::BlendState>(blendMode);

  if (p < 0.40f) {
    float wireThreshold = 1.0f - (p / 0.40f);
    Object3dCommon::GetInstance()->SetCommonDrawSettings(blend, true);
    obj->DrawWireframe(wireThreshold, kWireframeColor, 0);
    Object3dCommon::GetInstance()->SetCommonDrawSettings(blend, false);
  } else if (p < 0.55f) {
    Object3dCommon::GetInstance()->SetCommonDrawSettings(blend, true);
    obj->DrawWireframe(0.0f, kWireframeColor, 0);
    Object3dCommon::GetInstance()->SetCommonDrawSettings(blend, false);
  } else {
    float solidThreshold = 1.0f - ((p - 0.55f) / 0.45f);
    obj->SetDissolveParams(1, solidThreshold, 0.05f, kTransitionEdgeColor);
    obj->Draw(0);

    Object3dCommon::GetInstance()->SetCommonDrawSettings(blend, true);
    obj->DrawWireframe(0.0f, kWireframeColor, 0);
    Object3dCommon::GetInstance()->SetCommonDrawSettings(blend, false);
  }
}

void ShootingTransitionDirector::DrawObjectWithClearTransition(Object3d* obj, float startTime, float duration,
                                                               bool isClearVignette, float phaseTimer, int blendMode) const {
  if (!obj) return;

  if (!isClearVignette || phaseTimer <= startTime) {
    obj->SetDissolveParams(0, 0.0f, 0.05f, kTransitionEdgeColor);
    obj->Draw(0);
    return;
  }

  if (phaseTimer >= startTime + duration) {
    return;
  }

  float p = (phaseTimer - startTime) / duration;
  auto blend = static_cast<BlendMode::BlendState>(blendMode);

  if (p < 0.45f) {
    float solidThreshold = p / 0.45f;
    obj->SetDissolveParams(1, solidThreshold, 0.05f, kTransitionEdgeColor);
    obj->Draw(0);

    Object3dCommon::GetInstance()->SetCommonDrawSettings(blend, true);
    obj->DrawWireframe(0.0f, kWireframeColor, 0);
    Object3dCommon::GetInstance()->SetCommonDrawSettings(blend, false);
  } else if (p < 0.60f) {
    Object3dCommon::GetInstance()->SetCommonDrawSettings(blend, true);
    obj->DrawWireframe(0.0f, kWireframeColor, 0);
    Object3dCommon::GetInstance()->SetCommonDrawSettings(blend, false);
  } else {
    float wireThreshold = (p - 0.60f) / 0.40f;
    Object3dCommon::GetInstance()->SetCommonDrawSettings(blend, true);
    obj->DrawWireframe(wireThreshold, kWireframeColor, 0);
    Object3dCommon::GetInstance()->SetCommonDrawSettings(blend, false);
  }
}

#ifdef USE_IMGUI
ShootingTransitionDirector::IntroImGuiActions ShootingTransitionDirector::UpdateIntroImGui(
    StageRailManager* railManager, ShootingWaveManager* waveManager, ShootingHUD* hud) {
  IntroImGuiActions actions;
  ImGui::Separator();
  if (ImGui::TreeNode("Intro Sequence & Swarm Anchor")) {
    ImGui::Checkbox("Preview End Camera", &isIntroPreviewMode_);
    if (ImGui::Button("Replay Intro")) {
      isIntroPreviewMode_ = false;
      if (railManager && waveManager) {
        Vector3 endRailPos = railManager->CalculateRailPosition(railManager->GetMaxProgress());
        Vector3 swarmCenter{ endRailPos.x + introSwarmOffset_.x, endRailPos.y + introSwarmOffset_.y, endRailPos.z + introSwarmOffset_.z };
        waveManager->RespawnIntroSwarm(introSwarmCount_, swarmCenter, introSwarmRadius_);
      }
      actions.replayIntro = true;
    }

    ImGui::Separator();
    ImGui::Text("Swarm Drone Anchor Settings");
    bool swarmChanged = false;
    swarmChanged |= ImGui::DragFloat3("Swarm Offset from Rail End", &introSwarmOffset_.x, 0.5f);
    swarmChanged |= ImGui::SliderFloat("Spawn Radius", &introSwarmRadius_, 5.0f, 60.0f, "%.1f");
    int countInt = static_cast<int>(introSwarmCount_);
    if (ImGui::DragInt("Drone Count", &countInt, 5, 10, static_cast<int>(ShootingWaveManager::kMaxSwarmCount))) {
      introSwarmCount_ = static_cast<uint32_t>(countInt);
      swarmChanged = true;
    }
    if ((ImGui::Button("Respawn Swarm at Current Anchor") || swarmChanged) && railManager && waveManager) {
      Vector3 endRailPos = railManager->CalculateRailPosition(railManager->GetMaxProgress());
      Vector3 swarmCenter{ endRailPos.x + introSwarmOffset_.x, endRailPos.y + introSwarmOffset_.y, endRailPos.z + introSwarmOffset_.z };
      waveManager->RespawnIntroSwarm(introSwarmCount_, swarmCenter, introSwarmRadius_);
    }

    if (hud && hud->UpdateReadyGoImGui()) {
      actions.playReadyGoOnly = true;
    }

    ImGui::Separator();
    ImGui::Text("End Camera Settings");
    ImGui::SliderFloat("Camera Stop Offset", &introCameraStopOffset_, 0.0f, 50.0f, "%.1f");
    ImGui::SliderFloat("Look-up Pitch", &introCameraPitch_, -1.5f, 1.5f, "%.2f rad");
    ImGui::SliderFloat("Look-around Yaw", &introCameraYaw_, -3.14f, 3.14f, "%.2f rad");

    if (railManager) {
      Vector3 endRailPos = railManager->CalculateRailPosition(railManager->GetMaxProgress());
      Vector3 swarmCenter{ endRailPos.x + introSwarmOffset_.x, endRailPos.y + introSwarmOffset_.y, endRailPos.z + introSwarmOffset_.z };
      Vector3 camPos = railManager->CalculateRailPosition(railManager->GetMaxProgress() - introCameraStopOffset_);
      ImGui::Separator();
      ImGui::Text("Rail End Pos: (%.1f, %.1f, %.1f)", endRailPos.x, endRailPos.y, endRailPos.z);
      ImGui::Text("Cam Stop Pos: (%.1f, %.1f, %.1f)", camPos.x, camPos.y, camPos.z);
      ImGui::Text("Swarm Center: (%.1f, %.1f, %.1f)", swarmCenter.x, swarmCenter.y, swarmCenter.z);
    }

    ImGui::TreePop();
  }
  return actions;
}
#endif // USE_IMGUI
