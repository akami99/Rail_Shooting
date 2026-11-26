#include "ShootingPlayerController.h"
#include "ApplicationConfig.h"
#include "Camera.h"
#include "Input.h"
#include "MathUtils.h"
#include "MatrixGenerators.h"
#include "ShootingEffectManager.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "imgui.h"
#endif // USE_IMGUI

using namespace MathUtils;
using namespace MathGenerators;

void ShootingPlayerController::Initialize(const std::string& crosshairPath) {
  TextureManager::GetInstance()->LoadTexture(crosshairPath);
  TextureManager::GetInstance()->LoadTexture("white.png");
  TextureManager::GetInstance()->LoadTexture("ui/weaponBG.png");
  TextureManager::GetInstance()->LoadTexture("ui/weaponSelectCursor.png");
  TextureManager::GetInstance()->LoadTexture("ui/hundgunIcon.png");
  TextureManager::GetInstance()->LoadTexture("ui/machinegunIcon.png");
  TextureManager::GetInstance()->LoadTexture("ui/machinegunAmmoIcon.png");
  TextureManager::GetInstance()->LoadTexture("ui/machinegunAmmoGetIcon.png");

  // 照準スプライト
  crosshair_ = std::make_unique<Sprite>();
  crosshair_->Initialize(crosshairPath);
  crosshair_->SetAnchorPoint({0.5f, 0.5f});
  crosshair_->SetScale({64.0f, 64.0f});
  crosshair_->SetColor(crosshairSettings_.colorNoAim);

  // 弾薬UI
  ammoBg_ = std::make_unique<Sprite>();
  ammoBg_->Initialize("white.png");
  ammoBg_->SetTranslate({30.0f, 640.0f});
  ammoBg_->SetScale({200.0f, 40.0f});
  ammoBg_->SetColor({0.2f, 0.2f, 0.2f, 0.8f});

  ammoUnits_.clear();
  ammoCurrentX_.clear();
  ammoTargetX_.clear();
  for (int i = 0; i < kMaxAmmo; ++i) {
    auto unit = std::make_unique<Sprite>();
    unit->Initialize("white.png");
    unit->SetScale({12.0f, 24.0f});
    unit->SetColor({183.0f / 255.0f, 132.0f / 255.0f, 48.0f / 255.0f, 1.0f});

    float initX = 202.0f - i * 20.0f;
    ammoUnits_.push_back(std::move(unit));
    ammoCurrentX_.push_back(initX);
    ammoTargetX_.push_back(initX);
  }

  // マシンガン残弾ゲージUI
  machineGunAmmoBar_ = std::make_unique<Sprite>();
  machineGunAmmoBar_->Initialize("white.png");
  machineGunAmmoBar_->SetAnchorPoint({0.0f, 0.5f});
  machineGunAmmoBar_->SetTranslate({35.0f, 660.0f});
  machineGunAmmoBar_->SetScale({0.0f, 20.0f});
  machineGunAmmoBar_->SetColor({1.0f, 0.65f, 0.15f, 1.0f});

  // 武器選択UI
  weaponBg_ = std::make_unique<Sprite>();
  weaponBg_->Initialize("ui/weaponBG.png");
  weaponBg_->SetAnchorPoint({0.5f, 0.5f});
  weaponBg_->SetTranslate(weaponBgPos_);
  weaponBg_->SetScale(weaponBgSize_);
  weaponBg_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});

  weaponSelectCursor_ = std::make_unique<Sprite>();
  weaponSelectCursor_->Initialize("ui/weaponSelectCursor.png");
  weaponSelectCursor_->SetAnchorPoint({0.5f, 0.5f});
  weaponSelectCursor_->SetTranslate(weaponHandgunPos_);
  weaponSelectCursor_->SetScale(weaponCursorSize_);
  weaponSelectCursor_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});

  weaponHandgunIcon_ = std::make_unique<Sprite>();
  weaponHandgunIcon_->Initialize("ui/hundgunIcon.png");
  weaponHandgunIcon_->SetAnchorPoint({0.5f, 0.5f});
  weaponHandgunIcon_->SetTranslate(weaponHandgunPos_);
  weaponHandgunIcon_->SetScale(weaponIconSize_);
  weaponHandgunIcon_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});

  weaponMachineGunIcon_ = std::make_unique<Sprite>();
  weaponMachineGunIcon_->Initialize("ui/machinegunIcon.png");
  weaponMachineGunIcon_->SetAnchorPoint({0.5f, 0.5f});
  weaponMachineGunIcon_->SetTranslate(weaponMachineGunPos_);
  weaponMachineGunIcon_->SetScale(weaponIconSize_);
  weaponMachineGunIcon_->SetColor({0.5f, 0.5f, 0.5f, 0.25f});

  weaponMachineGunAmmoGetIcon_ = std::make_unique<Sprite>();
  weaponMachineGunAmmoGetIcon_->Initialize("ui/machinegunAmmoGetIcon.png");
  weaponMachineGunAmmoGetIcon_->SetAnchorPoint({0.5f, 0.5f});
  weaponMachineGunAmmoGetIcon_->SetTranslate(weaponMachineGunPos_);
  weaponMachineGunAmmoGetIcon_->SetScale(weaponIconSize_);
  weaponMachineGunAmmoGetIcon_->SetColor({1.0f, 1.0f, 1.0f, 0.0f});

  ResetState(true);
}

void ShootingPlayerController::Finalize() {
  ammoDropEffects_.clear();
  weaponMachineGunAmmoGetIcon_.reset();
  ammoGetEffectTimer_ = 0.0f;
}

void ShootingPlayerController::ResetState(bool resetMachineGunAmmo) {
  hp_ = kMaxHp;
  currentWeapon_ = WeaponType::Handgun;
  ammo_ = kMaxAmmo;
  if (resetMachineGunAmmo) {
    machineGunAmmo_ = 0;
  }
  shootCooldownTimer_ = 0.0f;
  ammoGetEffectTimer_ = 0.0f;
  ammoDropEffects_.clear();
  isCovering_ = false;
  coverYOffset_ = 0.0f;
  reloadTimer_ = 0.0f;
  lastReloadCount_ = 0;
}

void ShootingPlayerController::ResetAmmoUnitsLayout() {
  for (int i = 0; i < kMaxAmmo; ++i) {
    float initX = 202.0f - i * 20.0f;
    ammoCurrentX_[i] = initX;
    ammoTargetX_[i] = initX;
    ammoUnits_[i]->SetTranslate({initX, 648.0f});
    ammoUnits_[i]->SetColor({183.0f / 255.0f, 132.0f / 255.0f, 48.0f / 255.0f, 1.0f});
    ammoUnits_[i]->Update();
  }
}

void ShootingPlayerController::ResetCover() {
  isCovering_ = false;
  coverYOffset_ = 0.0f;
  reloadTimer_ = 0.0f;
}

void ShootingPlayerController::SetForceCover(bool covering) {
  isCovering_ = covering;
  coverYOffset_ = covering ? coverDepth_ : 0.0f;
  if (!covering) {
    reloadTimer_ = 0.0f;
  }
}

void ShootingPlayerController::TickInvincibleTimer(float deltaTime) {
  if (sectionJumpInvincibleTimer_ > 0.0f) {
    sectionJumpInvincibleTimer_ -= deltaTime;
    if (sectionJumpInvincibleTimer_ < 0.0f) {
      sectionJumpInvincibleTimer_ = 0.0f;
    }
  }
}

void ShootingPlayerController::UpdateCoverAndWeapon(bool isDemoMode, bool demoIsCovering, Camera* camera, ShootingEffectManager* effectManager) {
  if (shootCooldownTimer_ > 0.0f) {
    shootCooldownTimer_ -= ApplicationConfig::kDeltaTime;
    if (shootCooldownTimer_ < 0.0f) {
      shootCooldownTimer_ = 0.0f;
    }
  }

  // 右クリックによる武器切り替え（ハンドガン ⇔ マシンガン）
  if (Input::GetInstance()->IsMouseButtonTriggered(1)) {
    currentWeapon_ = (currentWeapon_ == WeaponType::Handgun) ? WeaponType::MachineGun : WeaponType::Handgun;
  }

  // 隠れるアクションの更新
  isCovering_ = isDemoMode ? demoIsCovering : Input::GetInstance()->IsKeyDown(DIK_SPACE);
  if (isCovering_) {
    if (ammo_ < kMaxAmmo) {
      reloadTimer_ += ApplicationConfig::kDeltaTime;
      if (reloadTimer_ >= kReloadDuration) {
        ammo_ = kMaxAmmo;
        reloadTimer_ = 0.0f;

        if (camera && effectManager) {
          Vector3 camPos = camera->GetTranslate();
          Vector3 camRot = camera->GetRotate();
          Vector3 forward = {std::sin(camRot.y), 0.0f, std::cos(camRot.y)};
          Vector3 spawnPos = Add(camPos, Multiply(1.5f, forward));
          effectManager->EmitReloadComplete(spawnPos);
        }
      }
    }
  } else {
    reloadTimer_ = 0.0f;
  }

  // カバーYオフセットのイージング更新
  float targetOffset = isCovering_ ? coverDepth_ : 0.0f;
  coverYOffset_ += (targetOffset - coverYOffset_) * coverEasingSpeed_;
}

void ShootingPlayerController::UpdateCrosshair(const Vector2& mousePos, bool isAiming) {
  if (!crosshair_) return;
  crosshair_->SetTranslate(mousePos);
  crosshair_->SetColor(isAiming ? crosshairSettings_.colorAim : crosshairSettings_.colorNoAim);
  crosshair_->Update();
}

void ShootingPlayerController::FadeCrosshair(float remainAlpha) {
  if (!crosshair_) return;
  Vector4 cCol = crosshairSettings_.colorNoAim;
  cCol.w *= remainAlpha;
  crosshair_->SetColor(cCol);
}

bool ShootingPlayerController::TryFire(bool isDemoMode, bool demoShootTriggered) {
  if (isCovering_ || shootCooldownTimer_ > 0.0f) {
    return false;
  }

  if (currentWeapon_ == WeaponType::Handgun) {
    bool triggerShot = isDemoMode ? demoShootTriggered : Input::GetInstance()->IsMouseButtonTriggered(0);
    if (triggerShot && (ammo_ > 0 || isDebugInfiniteAmmo_)) {
      shootCooldownTimer_ = kHandgunFireInterval;
      if (!isDebugInfiniteAmmo_) {
        ammo_--;
      }
      return true;
    }
  } else if (currentWeapon_ == WeaponType::MachineGun) {
    bool downShot = isDemoMode ? demoShootTriggered : Input::GetInstance()->IsMouseButtonDown(0);
    if (downShot && (machineGunAmmo_ > 0 || isDebugInfiniteAmmo_)) {
      shootCooldownTimer_ = kMachineGunFireInterval;
      if (!isDebugInfiniteAmmo_) {
        machineGunAmmo_--;
      }
      return true;
    }
  }

  return false;
}

bool ShootingPlayerController::ApplyDamage(int damage) {
  hp_ = (std::max)(0, hp_ - damage);
  return hp_ <= 0;
}

void ShootingPlayerController::OnEliteEnemyDefeated(const Vector3& worldPos) {
  machineGunAmmo_ = (std::min)(kMaxMachineGunAmmo, machineGunAmmo_ + kEliteDropMachineGunAmmo);

  MachineGunAmmoDropEffect drop;
  drop.baseWorldPos = Add(worldPos, {0.0f, 1.8f, 0.0f});
  drop.timer = 0.0f;
  drop.sprite = std::make_unique<Sprite>();
  drop.sprite->Initialize("ui/machinegunAmmoIcon.png");
  drop.sprite->SetAnchorPoint({0.5f, 0.5f});
  drop.sprite->SetScale({64.0f, 32.0f});
  ammoDropEffects_.push_back(std::move(drop));

  if (ammoGetEffectTimer_ <= 0.0f) {
    ammoGetEffectTimer_ = 2.0f;
  }
}

void ShootingPlayerController::UpdateUI(const Camera& camera, float clientWidth, float clientHeight) {
  // 1. 弾薬UIの更新
  bool isReloading = isCovering_ && (ammo_ < kMaxAmmo);

  if (isReloading) {
    float t = reloadTimer_ / (kReloadDuration * 0.85f);
    if (t > 1.0f) t = 1.0f;
    int reloadCount = static_cast<int>(t * kMaxAmmo);
    float blinkAlpha = 0.4f + 0.6f * std::abs(std::sin(reloadTimer_ * 20.0f));

    if (reloadCount > lastReloadCount_) {
      for (int i = 9 - reloadCount; i <= 9 - 1 - lastReloadCount_; ++i) {
        if (i >= 0 && i < kMaxAmmo) {
          ammoCurrentX_[i] = 202.0f;
        }
      }
    }
    lastReloadCount_ = reloadCount;

    for (int i = 0; i < kMaxAmmo; ++i) {
      if (i >= 9 - reloadCount) {
        ammoTargetX_[i] = 202.0f - (i + reloadCount - 9) * 20.0f;
        ammoCurrentX_[i] += (ammoTargetX_[i] - ammoCurrentX_[i]) * 0.15f;
        ammoUnits_[i]->SetTranslate({ammoCurrentX_[i], 648.0f});
        ammoUnits_[i]->SetColor({183.0f / 255.0f, 132.0f / 255.0f, 48.0f / 255.0f, blinkAlpha});
      } else {
        ammoUnits_[i]->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
      }
      ammoUnits_[i]->Update();
    }
  } else {
    lastReloadCount_ = 0;
    if (currentWeapon_ == WeaponType::MachineGun) {
      for (int i = 0; i < kMaxAmmo; ++i) {
        ammoUnits_[i]->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
        ammoUnits_[i]->Update();
      }
    } else {
      int currentAmmo = ammo_;
      for (int i = 0; i < kMaxAmmo; ++i) {
        if (i >= 9 - currentAmmo) {
          ammoTargetX_[i] = 202.0f - (i - 9 + currentAmmo) * 20.0f;
          ammoCurrentX_[i] += (ammoTargetX_[i] - ammoCurrentX_[i]) * 0.15f;
          ammoUnits_[i]->SetTranslate({ammoCurrentX_[i], 648.0f});
          ammoUnits_[i]->SetColor({183.0f / 255.0f, 132.0f / 255.0f, 48.0f / 255.0f, 1.0f});
        } else {
          ammoUnits_[i]->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
        }
        ammoUnits_[i]->Update();
      }
    }
  }
  if (ammoBg_) {
    ammoBg_->Update();
  }

  if (machineGunAmmoBar_) {
    if (currentWeapon_ == WeaponType::MachineGun) {
      float ammoRatio = static_cast<float>(machineGunAmmo_) / static_cast<float>(kMaxMachineGunAmmo);
      ammoRatio = std::clamp(ammoRatio, 0.0f, 1.0f);
      machineGunAmmoBar_->SetScale({190.0f * ammoRatio, 24.0f});
      machineGunAmmoBar_->SetColor({1.0f, 0.65f, 0.15f, 1.0f});
    } else {
      machineGunAmmoBar_->SetScale({0.0f, 0.0f});
      machineGunAmmoBar_->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
    }
    machineGunAmmoBar_->Update();
  }

  // 2. 武器選択UIの更新
  if (weaponBg_) {
    weaponBg_->SetTranslate(weaponBgPos_);
    weaponBg_->SetScale(weaponBgSize_);
    weaponBg_->Update();
  }

  if (weaponSelectCursor_) {
    Vector2 targetCursorPos = (currentWeapon_ == WeaponType::Handgun) ? weaponHandgunPos_ : weaponMachineGunPos_;
    weaponSelectCursor_->SetTranslate(targetCursorPos);
    weaponSelectCursor_->SetScale(weaponCursorSize_);
    weaponSelectCursor_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    weaponSelectCursor_->Update();
  }

  if (weaponHandgunIcon_) {
    weaponHandgunIcon_->SetTranslate(weaponHandgunPos_);
    weaponHandgunIcon_->SetScale(weaponIconSize_);
    weaponHandgunIcon_->SetColor(ammo_ > 0 ? Vector4{1.0f, 1.0f, 1.0f, 1.0f} : Vector4{0.5f, 0.5f, 0.5f, 0.25f});
    weaponHandgunIcon_->Update();
  }

  if (weaponMachineGunIcon_) {
    weaponMachineGunIcon_->SetTranslate(weaponMachineGunPos_);
    weaponMachineGunIcon_->SetScale(weaponIconSize_);
    weaponMachineGunIcon_->SetColor(machineGunAmmo_ > 0 ? Vector4{1.0f, 1.0f, 1.0f, 1.0f} : Vector4{0.5f, 0.5f, 0.5f, 0.25f});
    weaponMachineGunIcon_->Update();
  }

  if (weaponMachineGunAmmoGetIcon_ && ammoGetEffectTimer_ > 0.0f) {
    ammoGetEffectTimer_ -= ApplicationConfig::kDeltaTime;
    if (ammoGetEffectTimer_ < 0.0f) {
      ammoGetEffectTimer_ = 0.0f;
    }
    weaponMachineGunAmmoGetIcon_->SetTranslate(weaponMachineGunPos_);
    weaponMachineGunAmmoGetIcon_->SetScale(weaponIconSize_);
    float blinkPhase = std::fmod(2.0f - ammoGetEffectTimer_, 0.20f);
    float alpha = (blinkPhase < 0.10f) ? 1.0f : 0.0f;
    weaponMachineGunAmmoGetIcon_->SetColor({1.0f, 1.0f, 1.0f, alpha});
    weaponMachineGunAmmoGetIcon_->Update();
  }

  // 3. エリート兵撃破時の弾薬ドロップ頭上アイコン演出の更新
  const Matrix4x4& vp = camera.GetViewProjectionMatrix();
  const Vector3& camPos = camera.GetTranslate();
  const Vector3& camRot = camera.GetRotate();
  Vector3 camForward = Normalize(TransformVector({0.0f, 0.0f, 1.0f}, MakeRotateXYZMatrix(camRot)));
  Vector3 camRight = {std::cos(camRot.y), 0.0f, -std::sin(camRot.y)};

  for (auto it = ammoDropEffects_.begin(); it != ammoDropEffects_.end();) {
    it->timer += ApplicationConfig::kDeltaTime;
    float t = it->timer;
    if (t >= 1.4f) {
      it = ammoDropEffects_.erase(it);
      continue;
    }

    float tMove = (std::min)(t, 0.55f);
    float yOffset = 3.6f * tMove - 4.0f * tMove * tMove;
    float xOffset = 0.35f * tMove;

    Vector3 currentWorldPos = {
        it->baseWorldPos.x + camRight.x * xOffset,
        it->baseWorldPos.y + yOffset,
        it->baseWorldPos.z + camRight.z * xOffset
    };

    float alpha = 1.0f;
    if (t > 0.85f) {
      alpha = 1.0f - (t - 0.85f) / (1.4f - 0.85f);
      if (alpha < 0.0f) alpha = 0.0f;
    }

    it->isVisible = false;
    Vector3 toPos = Subtract(currentWorldPos, camPos);
    if (Dot(toPos, camForward) > 0.1f && it->sprite) {
      Vector3 ndc = TransformPoint(currentWorldPos, vp);
      if (ndc.z >= 0.0f && ndc.z <= 1.0f) {
        float screenX = (ndc.x * 0.5f + 0.5f) * clientWidth;
        float screenY = (1.0f - (ndc.y * 0.5f + 0.5f)) * clientHeight;

        it->sprite->SetTranslate({screenX, screenY});
        it->sprite->SetScale({64.0f, 32.0f});
        it->sprite->SetColor({1.0f, 1.0f, 1.0f, alpha});
        it->sprite->Update();
        it->isVisible = true;
      }
    }
    ++it;
  }
}

void ShootingPlayerController::DrawAmmoAndWeaponUI() {
  if (ammoBg_) {
    ammoBg_->Draw();
  }
  if (currentWeapon_ == WeaponType::MachineGun) {
    if (machineGunAmmoBar_) {
      machineGunAmmoBar_->Draw();
    }
  } else {
    for (int i = 0; i < kMaxAmmo; ++i) {
      if (ammoUnits_[i]) {
        ammoUnits_[i]->Draw();
      }
    }
  }

  if (weaponBg_) weaponBg_->Draw();
  if (weaponSelectCursor_) weaponSelectCursor_->Draw();
  if (weaponHandgunIcon_) weaponHandgunIcon_->Draw();
  if (weaponMachineGunIcon_) weaponMachineGunIcon_->Draw();
  if (ammoGetEffectTimer_ > 0.0f && weaponMachineGunAmmoGetIcon_) {
    weaponMachineGunAmmoGetIcon_->Draw();
  }
}

void ShootingPlayerController::DrawDropEffects() {
  for (auto& drop : ammoDropEffects_) {
    if (drop.isVisible && drop.sprite) {
      drop.sprite->Draw();
    }
  }
}

void ShootingPlayerController::DrawCrosshair() {
  if (crosshair_) {
    crosshair_->Draw();
  }
}

bool ShootingPlayerController::IsInvincible(bool isAutoClearing) const {
  if (isAutoClearing || isDebugInvincible_ || sectionJumpInvincibleTimer_ > 0.0f) {
    return true;
  }
  if (isCovering_ && coverYOffset_ <= coverYTarget_) {
    return true;
  }
  return false;
}

bool ShootingPlayerController::HasCurrentWeaponAmmo() const {
  int currentAmmo = (currentWeapon_ == WeaponType::Handgun) ? ammo_ : machineGunAmmo_;
  return currentAmmo > 0 || isDebugInfiniteAmmo_;
}

#ifdef USE_IMGUI
void ShootingPlayerController::UpdateGlobalDebugImGui() {
  ImGui::Text("Player HP: %d / %d", hp_, kMaxHp);
  if (ImGui::Button("Reset Player HP")) {
    hp_ = kMaxHp;
  }
  ImGui::Text(hp_ > 0 ? "Player is Alive" : "Player is Dead");
  ImGui::Separator();
  ImGui::Text("Ammo: %d / %d", ammo_, kMaxAmmo);
  ImGui::Text("Covering (SPACE): %s", isCovering_ ? "YES (Invincible)" : "NO");
  if (isCovering_ && ammo_ < kMaxAmmo) {
    ImGui::ProgressBar(reloadTimer_ / kReloadDuration, ImVec2(0.0f, 0.0f), "Reloading...");
  }
  ImGui::Checkbox("Debug Infinite Ammo", &isDebugInfiniteAmmo_);
  ImGui::Checkbox("Debug Invincible", &isDebugInvincible_);
  if (sectionJumpInvincibleTimer_ > 0.0f) {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Jump Invincible: %.2fs", sectionJumpInvincibleTimer_);
  }
}

void ShootingPlayerController::UpdateCoverImGui(bool isAutoClearing) {
  ImGui::Text("Player Height & Cover");
  ImGui::DragFloat("Eye Height (m)", &eyeHeight_, 0.05f, 0.0f, 3.0f, "%.2f");
  if (ImGui::DragFloat("Cover Depth (m)", &coverDepth_, 0.05f, -2.5f, -0.1f, "%.2f")) {
    coverYTarget_ = coverDepth_ * 0.8f;
  }
  ImGui::DragFloat("Cover Easing Speed", &coverEasingSpeed_, 0.01f, 0.02f, 0.5f, "%.2f");
  ImGui::Text("Cover Offset: %.2f (Invincible: %s)", coverYOffset_, IsInvincible(isAutoClearing) ? "YES" : "NO");
}

void ShootingPlayerController::UpdateStatusImGui() {
  ImGui::Text("Weapon: %s", (currentWeapon_ == WeaponType::Handgun) ? "Handgun" : "MachineGun");
  ImGui::Text("Player HP: %d / %d", hp_, kMaxHp);
  ImGui::Text("Handgun Ammo: %d / %d", ammo_, kMaxAmmo);
  ImGui::Text("MachineGun Ammo: %d / %d", machineGunAmmo_, kMaxMachineGunAmmo);
  ImGui::Text("Covering (SPACE): %s", isCovering_ ? "YES (Invincible)" : "NO");
  if (isCovering_ && ammo_ < kMaxAmmo) {
    ImGui::ProgressBar(reloadTimer_ / kReloadDuration, ImVec2(0.0f, 0.0f), "Reloading...");
  }
}

void ShootingPlayerController::UpdateWeaponLayoutImGui() {
  if (ImGui::TreeNode("Weapon UI Layout")) {
    ImGui::DragFloat2("Bg Pos##Weapon", &weaponBgPos_.x, 1.0f);
    ImGui::DragFloat2("Bg Size##Weapon", &weaponBgSize_.x, 1.0f);
    ImGui::DragFloat2("Cursor Size##Weapon", &weaponCursorSize_.x, 1.0f);
    ImGui::DragFloat2("Handgun Pos##Weapon", &weaponHandgunPos_.x, 1.0f);
    ImGui::DragFloat2("MachineGun Pos##Weapon", &weaponMachineGunPos_.x, 1.0f);
    ImGui::DragFloat2("Icon Size##Weapon", &weaponIconSize_.x, 1.0f);
    ImGui::TreePop();
  }
}
#endif // USE_IMGUI
