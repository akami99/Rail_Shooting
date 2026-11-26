#include "ShootingDemoAI.h"
#include "ApplicationConfig.h"
#include "Camera.h"
#include "GameSession.h"
#include "MathUtils.h"
#include "TextureManager.h"
#include <cmath>

using namespace MathUtils;

Vector2 ShootingDemoAI::WorldToScreen(const Vector3& worldPos, const Camera* camera) const {
  if (!camera) return { 640.0f, 360.0f };
  const Matrix4x4& matViewProj = camera->GetViewProjectionMatrix();
  float x = worldPos.x * matViewProj.m[0][0] + worldPos.y * matViewProj.m[1][0] + worldPos.z * matViewProj.m[2][0] + matViewProj.m[3][0];
  float y = worldPos.x * matViewProj.m[0][1] + worldPos.y * matViewProj.m[1][1] + worldPos.z * matViewProj.m[2][1] + matViewProj.m[3][1];
  float w = worldPos.x * matViewProj.m[0][3] + worldPos.y * matViewProj.m[1][3] + worldPos.z * matViewProj.m[2][3] + matViewProj.m[3][3];
  if (std::abs(w) > 0.0001f) {
    x /= w;
    y /= w;
  }
  return { (x * 0.5f + 0.5f) * 1280.0f, (-y * 0.5f + 0.5f) * 720.0f };
}

void ShootingDemoAI::Initialize() {
  TextureManager::GetInstance()->LoadTexture("ui/pressSpaceText.png");
  pressSpaceSprite_ = std::make_unique<Sprite>();
  pressSpaceSprite_->Initialize("ui/pressSpaceText.png");
  pressSpaceSprite_->SetAnchorPoint({ 0.5f, 0.5f });
  pressSpaceSprite_->SetTranslate({ 640.0f, 580.0f });
  pressSpaceSprite_->SetScale({ 256.0f, 64.0f });
  pressSpaceSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

  blinkTimer_ = 0.0f;
  mousePos_ = { 640.0f, 360.0f };
  isCovering_ = false;
  isShootTriggered_ = false;
  wantsSwitchToHandgun_ = false;
  wantsSwitchToMachineGun_ = false;
  shootCooldown_ = 0.0f;
  swarmBattleTimer_ = 0.0f;
  minCoverTimer_ = 0.0f;
  groundShootTimer_ = 0.0f;
  groundShootCooldown_ = 3.0f;
  machineGunTimer_ = 0.0f;
  handgunTimer_ = 5.0f;
  needHandgunReload_ = false;
}

void ShootingDemoAI::UpdateWeaponRotation(const Context& ctx, bool& isMachineGun) {
  if (needHandgunReload_) {
    wantsSwitchToHandgun_ = true;
    isMachineGun = false;
    if (ctx.ammo >= ctx.maxAmmo) {
      needHandgunReload_ = false;
      handgunTimer_ = 5.0f;
    }
  } else if (!isMachineGun) {
    if (!isCovering_) {
      handgunTimer_ -= ApplicationConfig::kDeltaTime;
    }
    if ((handgunTimer_ <= 0.0f || ctx.ammo <= 0) && ctx.machineGunAmmo > 0) {
      wantsSwitchToMachineGun_ = true;
      isMachineGun = true;
      machineGunTimer_ = 5.0f;
    }
  } else {
    if (!isCovering_) {
      machineGunTimer_ -= ApplicationConfig::kDeltaTime;
    }
    if (machineGunTimer_ <= 0.0f || ctx.machineGunAmmo <= 0) {
      machineGunTimer_ = 0.0f;
      wantsSwitchToHandgun_ = true;
      isMachineGun = false;
      if (ctx.ammo < ctx.maxAmmo) {
        needHandgunReload_ = true;
      } else {
        handgunTimer_ = 5.0f;
      }
    }
  }
}

void ShootingDemoAI::Update(const Context& ctx) {
  wantsSwitchToHandgun_ = false;
  wantsSwitchToMachineGun_ = false;
  isShootTriggered_ = false;

  if (!GameSession::GetInstance().IsDemoMode()) return;

  // pressSpaceText の点滅タイマー更新
  blinkTimer_ += ApplicationConfig::kDeltaTime;
  if (pressSpaceSprite_) {
    float alpha = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(blinkTimer_ * 5.0f));
    pressSpaceSprite_->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
    pressSpaceSprite_->Update();
  }

  shootCooldown_ -= ApplicationConfig::kDeltaTime;
  if (minCoverTimer_ > 0.0f) {
    minCoverTimer_ -= ApplicationConfig::kDeltaTime;
  }

  bool isMachineGun = ctx.isMachineGun;

  // 1. 通常戦闘フェーズ (Playing / RestartSmoothing)
  if (ctx.mode == BattleMode::Playing) {
    UpdateWeaponRotation(ctx, isMachineGun);

    bool needReloadCover = needHandgunReload_ || (!isMachineGun && ctx.ammo <= 0 && ctx.machineGunAmmo <= 0);
    bool needDodgeCover = false;
    if (ctx.hasTargetBlastProj && ctx.minBlastProjDist <= 8.0f && ctx.playerHp <= ctx.directDamage) {
      needDodgeCover = true;
    }
    if (ctx.hasIncomingNormalProj) {
      needDodgeCover = true;
    }

    if (needReloadCover || needDodgeCover) {
      if (!isCovering_) {
        minCoverTimer_ = needReloadCover ? (ctx.reloadDuration + 0.15f) : 0.35f;
      }
      isCovering_ = true;
    } else if (minCoverTimer_ <= 0.0f) {
      isCovering_ = false;
    }

    // たまに地面を撃つタイマー更新（着弾エフェクトを見せるため）
    if (!isCovering_ && !ctx.hasTargetBlastProj) {
      if (groundShootTimer_ > 0.0f) {
        groundShootTimer_ -= ApplicationConfig::kDeltaTime;
      } else {
        groundShootCooldown_ -= ApplicationConfig::kDeltaTime;
        if (groundShootCooldown_ <= 0.0f) {
          groundShootTimer_ = 0.45f;
          groundShootCooldown_ = 4.0f;
        }
      }
    }

    // 照準移動と射撃（優先度: 爆発弾迎撃 > 地面撃ち > 敵攻撃 > 索敵）
    if (ctx.hasTargetBlastProj && !isCovering_) {
      Vector2 targetScreenPos = WorldToScreen(ctx.targetBlastProjPos, ctx.camera);
      mousePos_.x += (targetScreenPos.x - mousePos_.x) * 0.30f;
      mousePos_.y += (targetScreenPos.y - mousePos_.y) * 0.30f;

      float dist = Length(Vector3{ mousePos_.x - targetScreenPos.x, mousePos_.y - targetScreenPos.y, 0.0f });
      if (!ctx.isCovering && dist < 50.0f && shootCooldown_ <= 0.0f) {
        isShootTriggered_ = true;
        shootCooldown_ = isMachineGun ? ctx.machineGunFireInterval : 0.22f;
      }
    } else if (groundShootTimer_ > 0.0f && !isCovering_ && ctx.camera) {
      Vector3 camPos = ctx.camera->GetTranslate();
      Vector3 camRot = ctx.camera->GetRotate();
      Vector3 forward = { std::sin(camRot.y), 0.0f, std::cos(camRot.y) };
      Vector3 groundTarget = {
          camPos.x + forward.x * 10.0f,
          0.0f,
          camPos.z + forward.z * 10.0f
      };
      Vector2 targetScreenPos = WorldToScreen(groundTarget, ctx.camera);
      mousePos_.x += (targetScreenPos.x - mousePos_.x) * 0.25f;
      mousePos_.y += (targetScreenPos.y - mousePos_.y) * 0.25f;

      float dist = Length(Vector3{ mousePos_.x - targetScreenPos.x, mousePos_.y - targetScreenPos.y, 0.0f });
      if (!ctx.isCovering && dist < 50.0f && shootCooldown_ <= 0.0f) {
        isShootTriggered_ = true;
        shootCooldown_ = isMachineGun ? ctx.machineGunFireInterval : 0.25f;
      }
    } else if (ctx.hasTargetEnemy) {
      Vector3 enemyWorldPos = ctx.targetEnemyPos;
      enemyWorldPos.y += 0.7f; // 胸〜頭部を狙う
      Vector2 targetScreenPos = WorldToScreen(enemyWorldPos, ctx.camera);

      float aimSpeed = isMachineGun ? 0.20f : 0.15f;
      mousePos_.x += (targetScreenPos.x - mousePos_.x) * aimSpeed;
      mousePos_.y += (targetScreenPos.y - mousePos_.y) * aimSpeed;

      // わずかな自然な揺らぎ
      mousePos_.x += std::sin(ctx.gameTimer * 5.0f) * 1.5f;
      mousePos_.y += std::cos(ctx.gameTimer * 4.0f) * 1.5f;

      float dist = Length(Vector3{ mousePos_.x - targetScreenPos.x, mousePos_.y - targetScreenPos.y, 0.0f });
      if (!ctx.isCovering && dist < 45.0f && shootCooldown_ <= 0.0f) {
        isShootTriggered_ = true;
        shootCooldown_ = isMachineGun ? ctx.machineGunFireInterval : 0.35f;
      }
    } else {
      // 敵がいない移動中：前方を索敵するように中央付近を見回す
      mousePos_.x += (640.0f + std::sin(ctx.gameTimer * 1.5f) * 60.0f - mousePos_.x) * 0.05f;
      mousePos_.y += (360.0f + std::cos(ctx.gameTimer * 1.2f) * 30.0f - mousePos_.y) * 0.05f;
    }
  }
  // 2. ボス戦フェーズ (SwarmBattle)
  else if (ctx.mode == BattleMode::SwarmBattle) {
    swarmBattleTimer_ += ApplicationConfig::kDeltaTime;

    if (ctx.bossPhase <= 1) {
      UpdateWeaponRotation(ctx, isMachineGun);

      bool needReloadCover = needHandgunReload_ || (!isMachineGun && ctx.ammo <= 0 && ctx.machineGunAmmo <= 0);
      if (needReloadCover) {
        if (!isCovering_) {
          minCoverTimer_ = ctx.reloadDuration + 0.15f;
        }
        isCovering_ = true;
      } else if (minCoverTimer_ <= 0.0f) {
        isCovering_ = false;
      }

      if (ctx.hasSwarmTarget) {
        Vector2 targetScreenPos = WorldToScreen(ctx.swarmTargetPos, ctx.camera);
        mousePos_.x += (targetScreenPos.x - mousePos_.x) * 0.25f;
        mousePos_.y += (targetScreenPos.y - mousePos_.y) * 0.25f;
      } else {
        mousePos_.x += (640.0f + std::sin(swarmBattleTimer_ * 3.0f) * 100.0f - mousePos_.x) * 0.15f;
        mousePos_.y += (280.0f + std::cos(swarmBattleTimer_ * 2.0f) * 50.0f - mousePos_.y) * 0.15f;
      }

      if (!ctx.isCovering && shootCooldown_ <= 0.0f) {
        isShootTriggered_ = true;
        shootCooldown_ = isMachineGun ? ctx.machineGunFireInterval : 0.22f;
      }
    } else {
      isCovering_ = false;
      mousePos_.x += (640.0f + std::sin(swarmBattleTimer_ * 1.5f) * 80.0f - mousePos_.x) * 0.08f;
      mousePos_.y += (260.0f + std::cos(swarmBattleTimer_ * 1.2f) * 40.0f - mousePos_.y) * 0.08f;
    }
  }
  // 3. ゲームオーバーフェーズ (GameOver)
  else if (ctx.mode == BattleMode::GameOver) {
    isCovering_ = false;
    mousePos_.x += (ctx.retryPos.x - mousePos_.x) * 0.2f;
    mousePos_.y += (ctx.retryPos.y - mousePos_.y) * 0.2f;

    float dist = Length(Vector3{ mousePos_.x - ctx.retryPos.x, mousePos_.y - ctx.retryPos.y, 0.0f });
    if (dist < 40.0f && shootCooldown_ <= 0.0f) {
      isShootTriggered_ = true;
      shootCooldown_ = 0.5f;
    }
  }
  // 4. その他のフェーズ (Intro 等)
  else {
    isCovering_ = false;
    mousePos_ = { 640.0f, 360.0f };
  }
}

void ShootingDemoAI::DrawUI() {
  if (GameSession::GetInstance().IsDemoMode() && pressSpaceSprite_) {
    pressSpaceSprite_->Draw();
  }
}
