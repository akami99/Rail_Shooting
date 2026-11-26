#include "ShootingWaveManager.h"
#include "ApplicationConfig.h"
#include "Camera.h"
#include "MathUtils.h"
#include "MatrixGenerators.h"
#include "Particle/ParticleManager.h"
#include "ShootingEffectManager.h"
#include "StageRailManager.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#ifdef USE_IMGUI
#include "imgui.h"
#endif // USE_IMGUI

using namespace MathUtils;
using namespace MathGenerators;

class ShootingWaveManager::NormalEnemyBehavior : public ShootingWaveManager::IEnemyBehavior {
public:
    EnemyType GetType() const override { return EnemyType::Normal; }
    int GetMaxHp() const override { return 1; }
    Vector4 GetModelColor() const override { return { 1.0f, 1.0f, 1.0f, 1.0f }; }
    float GetSpawnInterval(float normalInterval, float engineerInterval) const override {
        (void)engineerInterval;
        return normalInterval;
    }
    float GetHitShotRate(float baseRate) const override { return baseRate; }
    Vector4 GetChargeRingColor(int hitShotCount) const override {
        return ((hitShotCount + 1) % 2 == 0) ? Vector4{ 0.1f, 0.65f, 1.0f, 1.0f } : Vector4{ 1.0f, 0.0f, 0.0f, 1.0f };
    }
    EnemyProjectile::Type SelectHitProjectileType(int hitShotCount) const override {
        return (hitShotCount % 2 == 0) ? EnemyProjectile::Type::Jamming : EnemyProjectile::Type::Normal;
    }
    EnemyProjectile::Type SelectMissProjectileType() const override { return EnemyProjectile::Type::Normal; }
};

class ShootingWaveManager::EngineerEnemyBehavior final : public ShootingWaveManager::IEnemyBehavior {
public:
    EnemyType GetType() const override { return EnemyType::Engineer; }
    int GetMaxHp() const override { return 3; }
    Vector4 GetModelColor() const override { return { 1.0f, 0.8f, 0.2f, 1.0f }; }
    float GetSpawnInterval(float normalInterval, float engineerInterval) const override {
        (void)normalInterval;
        return engineerInterval;
    }
    float GetHitShotRate(float baseRate) const override {
        (void)baseRate;
        return 0.75f;
    }
    Vector4 GetChargeRingColor(int hitShotCount) const override {
        (void)hitShotCount;
        return { 1.0f, 0.8f, 0.1f, 1.0f };
    }
    EnemyProjectile::Type SelectHitProjectileType(int hitShotCount) const override {
        (void)hitShotCount;
        return EnemyProjectile::Type::Blast;
    }
    EnemyProjectile::Type SelectMissProjectileType() const override { return EnemyProjectile::Type::Blast; }
};

class ShootingWaveManager::EliteEnemyBehavior final : public ShootingWaveManager::NormalEnemyBehavior {
public:
    EnemyType GetType() const override { return EnemyType::Elite; }
    int GetMaxHp() const override { return 3; }
    Vector4 GetModelColor() const override { return { 1.0f, 0.2f, 0.2f, 1.0f }; }
    bool DropsMachineGunAmmoOnDeath() const override { return true; }
    void PrepareFirstShotIfNeeded(float chargeStart, float& shootTimer,
                                  bool& isNextHitShot, bool hasFiredFirstShot) const override {
        if (!hasFiredFirstShot && shootTimer < chargeStart) {
            shootTimer = chargeStart;
            isNextHitShot = true;
        }
    }
    bool ShouldMarkFirstShotFired() const override { return true; }
};

std::unique_ptr<ShootingWaveManager::IEnemyBehavior> ShootingWaveManager::CreateEnemyBehavior(EnemyType type) {
    switch (type) {
    case EnemyType::Engineer:
        return std::make_unique<EngineerEnemyBehavior>();
    case EnemyType::Elite:
        return std::make_unique<EliteEnemyBehavior>();
    case EnemyType::Normal:
    default:
        return std::make_unique<NormalEnemyBehavior>();
    }
}

void ShootingWaveManager::ApplyEnemyType(EnemyInfo& enemy, EnemyType type, bool resetHpAndGauge) const {
    enemy.behavior = CreateEnemyBehavior(type);
    if (enemy.model && enemy.behavior) {
        enemy.model->SetColor(enemy.behavior->GetModelColor());
    }
    if (resetHpAndGauge && enemy.behavior) {
        enemy.maxHp = enemy.behavior->GetMaxHp();
        enemy.hp = enemy.maxHp;
        enemy.damageBarRatio = 1.0f;
        enemy.isHpGaugeVisible = false;
    }
}

void ShootingWaveManager::Initialize(const LevelData* levelData,
                                     const StageRailManager& railManager,
                                     Camera* camera,
                                     const std::string& enemyModelDir,
                                     const std::string& enemyModelName,
                                     const Vector3& introSwarmOffset,
                                     float introSwarmRadius,
                                     uint32_t introSwarmCount) {
  enemies_.clear();
  projectiles_.clear();
  squadHitShotCount_ = 0;
  squadFiredHitShotCount_ = 0;
  squadHitShotCooldown_ = 0.0f;
  hasPendingSwarmAttack_ = false;
  swarmBattleTimer_ = 0.0f;
  hasSpawnedBossEntrance_ = false;
  bossDamageBarRatio_ = 1.0f;

  if (levelData) {
    for (const auto& enemyData : levelData->enemies) {
      EnemyInfo enemy;
      enemy.object = std::make_unique<Object3d>();
      enemy.object->Initialize();

      enemy.model = std::make_unique<Model>();
      enemy.model->Initialize(enemyModelDir, enemyModelName);
      enemy.model->SetDissolveMaskTexture("masks/noise0.png");
      enemy.model->SetDissolveParams(0, 0.0f, 0.05f, Vector3(1.0f, 0.4f, 0.3f));

      enemy.object->SetModel(enemy.model.get());
      enemy.object->SetTranslate(enemyData.translation);
      enemy.object->SetRotation(enemyData.rotation);
      enemy.object->SetCamera(camera);
      enemy.basePosition = enemyData.translation;
      enemy.baseRotation = enemyData.rotation;
      enemy.distance = enemyData.distance;

      EnemyType parsedType = EnemyType::Normal;
      if (enemyData.enemyType == "ENGINEER" || enemyData.enemyType == "Engineer") {
        parsedType = EnemyType::Engineer;
      } else if (enemyData.enemyType == "ELITE" || enemyData.enemyType == "Elite") {
        parsedType = EnemyType::Elite;
      }
      ApplyEnemyType(enemy, parsedType, false);

      enemy.isActive = false;
      enemy.isDead = false;
      enemy.shootTimer = 0.0f;
      enemy.spawnTimer = 0.0f;
      enemy.slideTimer = 0.0f;
      enemy.deathTimer = 0.0f;
      enemy.hasFiredEliteFirstShot = false;
      float rate = enemy.behavior->GetHitShotRate(hitShotRate_);
      enemy.isNextHitShot = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < rate);
      enemies_.push_back(std::move(enemy));
    }

    TextureManager::GetInstance()->LoadTexture("white.png");
    TextureManager::GetInstance()->LoadTexture("ui/enemyHpFrame.png");
    for (auto& enemy : enemies_) {
      enemy.maxHp = enemy.behavior->GetMaxHp();
      enemy.hp = enemy.maxHp;
      enemy.damageBarRatio = 1.0f;
      enemy.isHpGaugeVisible = false;

      enemy.hpBgSprite = std::make_unique<Sprite>();
      enemy.hpBgSprite->Initialize("white.png");
      enemy.hpBgSprite->SetAnchorPoint({0.0f, 0.5f});
      enemy.hpBgSprite->SetColor({0.08f, 0.08f, 0.08f, 0.85f});

      enemy.hpDamageSprite = std::make_unique<Sprite>();
      enemy.hpDamageSprite->Initialize("white.png");
      enemy.hpDamageSprite->SetAnchorPoint({0.0f, 0.5f});
      enemy.hpDamageSprite->SetColor({1.0f, 0.2f, 0.1f, 0.9f});

      enemy.hpFillSprite = std::make_unique<Sprite>();
      enemy.hpFillSprite->Initialize("white.png");
      enemy.hpFillSprite->SetAnchorPoint({0.0f, 0.5f});
      enemy.hpFillSprite->SetColor({1.0f, 0.85f, 0.1f, 1.0f});

      enemy.hpFrameSprite = std::make_unique<Sprite>();
      enemy.hpFrameSprite->Initialize("ui/enemyHpFrame.png");
      enemy.hpFrameSprite->SetAnchorPoint({0.5f, 0.5f});
      enemy.hpFrameSprite->SetScale({84.0f, 16.0f});
    }

    for (size_t i = 0; i < enemies_.size(); ++i) {
      auto& enemy = enemies_[i];
      Vector3 camPosAtBattle = railManager.CalculateRailPosition(enemy.distance);
      Vector3 camForward = {0.0f, 0.0f, 1.0f};
      Vector3 camUp = {0.0f, 1.0f, 0.0f};
      Vector3 camRight = Normalize(Cross(camUp, camForward));

      Vector3 toEnemy = Subtract(enemy.basePosition, camPosAtBattle);
      float sideDot = Dot(toEnemy, camRight);

      size_t localIdx = 0;
      for (size_t j = 0; j < i; ++j) {
        if (std::abs(enemies_[j].distance - enemy.distance) < 0.1f) {
          localIdx++;
        }
      }
      enemy.slideDelay = static_cast<float>(localIdx) * 0.16f;
      enemy.shootTimer = static_cast<float>((localIdx % 4) * 20);
      enemy.slideDuration = 0.85f;

      if (i % 4 == 3) {
        enemy.initialOffsetPosition = Add(enemy.basePosition, Add(Multiply(15.0f, camForward), Multiply(10.0f, camUp)));
      } else {
        float sign = (sideDot >= 0.0f) ? 1.0f : -1.0f;
        float offsetDist = 18.0f + static_cast<float>(i % 3) * 3.0f;
        enemy.initialOffsetPosition = Add(enemy.basePosition, Multiply(sign * offsetDist, camRight));
      }

      enemy.object->SetTranslate(enemy.initialOffsetPosition);
      enemy.object->Update(0, camera);
    }
  }

  // スウォームシステムの初期化
  swarmManager_ = std::make_unique<SwarmManager>();
  swarmManager_->Initialize(kMaxSwarmCount, enemyModelDir, enemyModelName);
  Vector3 endRailPos = railManager.CalculateRailPosition(railManager.GetMaxProgress());
  Vector3 swarmCenter{
      endRailPos.x + introSwarmOffset.x,
      endRailPos.y + introSwarmOffset.y,
      endRailPos.z + introSwarmOffset.z
  };
  swarmManager_->SpawnDrones(introSwarmCount, swarmCenter, introSwarmRadius);
  swarmManager_->SetAttackDistance(0.0f);

  // ボス登場演出・高高度漂い用1000機スウォームの初期化
  ambientSwarmManager_ = std::make_unique<SwarmManager>();
  ambientSwarmManager_->Initialize(kAmbientSwarmCount, enemyModelDir, enemyModelName);
  ambientSwarmManager_->SetAttackDistance(0.0f);
  ambientSwarmManager_->SetDroneScale(0.24f);

  // ボス全体HPゲージUIの初期化
  bossHpBgSprite_ = std::make_unique<Sprite>();
  bossHpBgSprite_->Initialize("white.png");
  bossHpBgSprite_->SetAnchorPoint({0.0f, 0.5f});
  bossHpBgSprite_->SetColor({0.08f, 0.08f, 0.08f, 0.85f});

  bossHpDamageSprite_ = std::make_unique<Sprite>();
  bossHpDamageSprite_->Initialize("white.png");
  bossHpDamageSprite_->SetAnchorPoint({0.0f, 0.5f});
  bossHpDamageSprite_->SetColor({1.0f, 0.2f, 0.1f, 0.9f});

  bossHpFillSprite_ = std::make_unique<Sprite>();
  bossHpFillSprite_->Initialize("white.png");
  bossHpFillSprite_->SetAnchorPoint({0.0f, 0.5f});
  bossHpFillSprite_->SetColor({0.2f, 0.95f, 0.7f, 1.0f});

  bossHpFrameSprite_ = std::make_unique<Sprite>();
  bossHpFrameSprite_->Initialize("ui/enemyHpFrame.png");
  bossHpFrameSprite_->SetAnchorPoint({0.5f, 0.5f});
  bossHpFrameSprite_->SetScale({380.0f, 22.0f});
}

void ShootingWaveManager::AddObserver(IShootingEventObserver* observer) {
  if (!observer) return;
  if (std::find(observers_.begin(), observers_.end(), observer) == observers_.end()) {
    observers_.push_back(observer);
  }
}

void ShootingWaveManager::RemoveObserver(IShootingEventObserver* observer) {
  observers_.erase(std::remove(observers_.begin(), observers_.end(), observer), observers_.end());
}

void ShootingWaveManager::NotifyScoreEarned(int scoreDelta) {
  if (scoreDelta <= 0) return;
  for (auto* observer : observers_) {
    if (observer) {
      observer->OnScoreEarned(scoreDelta);
    }
  }
}

void ShootingWaveManager::NotifyEliteEnemyDefeated(const Vector3& worldPos) {
  for (auto* observer : observers_) {
    if (observer) {
      observer->OnEliteEnemyDefeated(worldPos);
    }
  }
}

void ShootingWaveManager::Finalize() {
  observers_.clear();
  enemies_.clear();
  projectiles_.clear();
  for (auto& pool : projectilePools_) {
    pool.clear();
  }
  if (swarmManager_) {
    swarmManager_->ClearDrones();
  }
  if (ambientSwarmManager_) {
    ambientSwarmManager_->ClearDrones();
  }
}

void ShootingWaveManager::UpdateIntroSwarm(float deltaTime, const Vector3& swarmCenter, Camera* camera) {
  if (swarmManager_) {
    swarmManager_->Update(deltaTime, swarmCenter, camera, nullptr);
  }
}

void ShootingWaveManager::ResetSwarmToBossSpawn(const Vector3& basePos, float baseYaw) {
  if (swarmManager_) {
    swarmManager_->ClearDrones();
    swarmManager_->SetAttackDistance(20.0f);
    swarmManager_->StartBossBattle(kSpawnSwarmCount, basePos, baseYaw);
  }
}

void ShootingWaveManager::ClearSwarmDrones() {
  if (swarmManager_) {
    swarmManager_->ClearDrones();
  }
}

void ShootingWaveManager::ClearAmbientSwarmDrones() {
  if (ambientSwarmManager_) {
    ambientSwarmManager_->ClearDrones();
  }
}

void ShootingWaveManager::ClearProjectiles() {
  for (auto& p : projectiles_) {
    ReleaseProjectileToPool(std::move(p));
  }
  projectiles_.clear();
}

std::unique_ptr<EnemyProjectile> ShootingWaveManager::AcquireProjectile(
    EnemyProjectile::Type type, const Vector3& position, const Vector3& velocity) {
  size_t poolIndex = static_cast<size_t>(type);
  if (poolIndex < projectilePools_.size() && !projectilePools_[poolIndex].empty()) {
    auto proj = std::move(projectilePools_[poolIndex].back());
    projectilePools_[poolIndex].pop_back();
    proj->Reset(position, velocity);
    return proj;
  }
  return EnemyProjectile::Create(type, position, velocity);
}

void ShootingWaveManager::ReleaseProjectileToPool(std::unique_ptr<EnemyProjectile> projectile) {
  if (!projectile) return;
  size_t poolIndex = static_cast<size_t>(projectile->GetType());
  if (poolIndex < projectilePools_.size()) {
    projectilePools_[poolIndex].push_back(std::move(projectile));
  }
}

void ShootingWaveManager::UpdateAliveEnemiesTransforms(int mainViewIndex, Camera* camera) {
  for (auto& enemy : enemies_) {
    if (!enemy.isDead && enemy.object) {
      enemy.object->Update(mainViewIndex, camera);
    }
  }
}

void ShootingWaveManager::StartBossIntro(const Vector3& basePos, float baseYaw) {
  hasSpawnedBossEntrance_ = false;
  projectiles_.clear();
  if (swarmManager_) {
    swarmManager_->ClearDrones();
  }
  if (ambientSwarmManager_) {
    Vector3 forward = {std::sin(baseYaw), 0.0f, std::cos(baseYaw)};
    Vector3 rearSpawnPos = {
        basePos.x - forward.x * 75.0f,
        basePos.y + 16.0f,
        basePos.z - forward.z * 75.0f
    };
    ambientSwarmManager_->SetDroneScale(0.24f);
    ambientSwarmManager_->SetAttackDistance(0.0f);
    ambientSwarmManager_->SetMaxSpeed(42.0f);
    ambientSwarmManager_->SpawnDrones(kAmbientSwarmCount, rearSpawnPos, 28.0f);
  }
}

void ShootingWaveManager::UpdateBossIntro(float phaseTimer, float bossIntroTotalDuration,
                                          const Vector3& basePos, float baseYaw, Camera* camera) {
  if (ambientSwarmManager_) {
    Vector3 forward = {std::sin(baseYaw), 0.0f, std::cos(baseYaw)};
    Vector3 ambientAnchor{};
    if (phaseTimer < 1.80f) {
      ambientSwarmManager_->SetMaxSpeed(42.0f);
      ambientAnchor = {
          basePos.x + forward.x * 16.0f,
          basePos.y + 17.5f,
          basePos.z + forward.z * 16.0f
      };
    } else {
      ambientSwarmManager_->SetMaxSpeed(16.0f);
      ambientAnchor = {
          basePos.x + forward.x * 26.0f,
          basePos.y + 16.5f,
          basePos.z + forward.z * 26.0f
      };
    }
    ambientSwarmManager_->Update(ApplicationConfig::kDeltaTime, ambientAnchor, camera, nullptr);
  }

  if (phaseTimer >= 3.00f && !hasSpawnedBossEntrance_ && phaseTimer < bossIntroTotalDuration) {
    hasSpawnedBossEntrance_ = true;
    if (swarmManager_) {
      Vector3 spawnPos = CalculateBossEntranceAnchor(0.0f, basePos, baseYaw);
      swarmManager_->StartBossEntrance(kSpawnSwarmCount, basePos, baseYaw, spawnPos);
    }
  }
  if (hasSpawnedBossEntrance_ && phaseTimer < bossIntroTotalDuration && swarmManager_) {
    float entranceT = (phaseTimer - 3.00f) / (bossIntroTotalDuration - 3.00f);
    Vector3 anchorPos = CalculateBossEntranceAnchor(entranceT, basePos, baseYaw);
    swarmManager_->UpdateBossEntrance(ApplicationConfig::kDeltaTime, anchorPos, camera);
  }
}

void ShootingWaveManager::BeginSwarmBattle(const Vector3& basePos, float baseYaw) {
  swarmBattleTimer_ = 0.0f;
  bossDamageBarRatio_ = 1.0f;
  if (swarmManager_) {
    swarmManager_->StartBossBattle(kSpawnSwarmCount, basePos, baseYaw, hasSpawnedBossEntrance_);
  }
  if (ambientSwarmManager_) {
    ambientSwarmManager_->SetMaxSpeed(16.0f);
  }
}

void ShootingWaveManager::UpdateAmbientSwarm(float deltaTime, const Vector3& anchor, Camera* camera) {
  if (ambientSwarmManager_) {
    ambientSwarmManager_->Update(deltaTime, anchor, camera, nullptr);
  }
}

void ShootingWaveManager::UpdateBossDefeatLookDown(float phaseTimer, float deltaTime, const Vector3& basePos, float baseYaw,
                                                   Camera* camera, float& ioCameraYaw, float& ioCameraPitch) {
  if (swarmManager_ && camera) {
    Vector3 camPos = camera->GetTranslate();
    swarmManager_->Update(deltaTime, camPos, camera, nullptr);

    if (phaseTimer >= 1.3f) {
      float dt = std::clamp((phaseTimer - 1.3f) / 1.3f, 0.0f, 1.0f);
      swarmManager_->SetCrashDissolveThreshold(dt);
    }

    Vector3 crashCenter{};
    swarmManager_->GetCrashCenter(crashCenter);
    Vector3 toCrash = Subtract(crashCenter, camPos);
    float distXZ = std::sqrt(toCrash.x * toCrash.x + toCrash.z * toCrash.z);
    if (distXZ > 0.01f || std::abs(toCrash.y) > 0.01f) {
      float targetYaw = std::atan2(toCrash.x, toCrash.z);
      float targetPitch = std::clamp(-std::atan2(toCrash.y, (std::max)(distXZ, 0.1f)), -0.15f, 1.1f);

      float dYaw = targetYaw - ioCameraYaw;
      while (dYaw < -std::numbers::pi_v<float>) dYaw += 2.0f * std::numbers::pi_v<float>;
      while (dYaw > std::numbers::pi_v<float>) dYaw -= 2.0f * std::numbers::pi_v<float>;
      ioCameraYaw += dYaw * 0.07f;
      ioCameraPitch += (targetPitch - ioCameraPitch) * 0.07f;
    }
  }

  if (ambientSwarmManager_ && ambientSwarmManager_->IsActive()) {
    Vector3 forward = {std::sin(baseYaw), 0.0f, std::cos(baseYaw)};
    Vector3 rearAnchor = {
        basePos.x - forward.x * 90.0f,
        basePos.y + 18.0f,
        basePos.z - forward.z * 90.0f
    };
    ambientSwarmManager_->SetMaxSpeed(48.0f);
    ambientSwarmManager_->Update(deltaTime, rearAnchor, camera, nullptr);
    if (phaseTimer >= 1.3f) {
      float dt = std::clamp((phaseTimer - 1.3f) / 1.3f, 0.0f, 1.0f);
      ambientSwarmManager_->SetCrashDissolveThreshold(dt);
    }
  }
}

void ShootingWaveManager::UpdateRailEnemyTriggers(StageRailManager& railManager) {
  if (railManager.IsMovementPaused()) {
    bool allDead = true;
    for (const auto& enemy : enemies_) {
      if (enemy.isActive && !enemy.isDead) {
        allDead = false;
        break;
      }
    }
    if (allDead) {
      railManager.SetMovementPaused(false);
    }
  }

  if (!railManager.IsMovementPaused()) {
    float nextProgress = railManager.GetProgress() + railManager.GetCameraSpeed();
    bool triggered = false;
    float triggerDistance = nextProgress;

    for (auto& enemy : enemies_) {
      if (!enemy.isActive && !enemy.isDead) {
        if (nextProgress >= enemy.distance) {
          enemy.isActive = true;
          triggered = true;
          if (enemy.distance < triggerDistance) {
            triggerDistance = enemy.distance;
          }
        }
      }
    }

    if (triggered) {
      squadHitShotCount_ = 0;
      squadFiredHitShotCount_ = 0;
      squadHitShotCooldown_ = 0.0f;
      for (auto& enemy : enemies_) {
        if (!enemy.isActive && !enemy.isDead &&
            std::abs(enemy.distance - triggerDistance) < 0.01f) {
          enemy.isActive = true;
        }
      }
      railManager.SetProgress(triggerDistance);
      railManager.SetMovementPaused(true);
    } else {
      railManager.SetProgress(nextProgress);
      if (railManager.GetProgress() > railManager.GetMaxProgress()) {
        railManager.SetProgress(railManager.GetMaxProgress());
      }
    }
  }
}

void ShootingWaveManager::UpdateEnemiesAndProjectiles(float deltaTime, float railProgress,
                                                      const Vector3& playerPos, int mainViewIndex, Camera* camera) {
  ParticleManager::GetInstance()->ClearParticles("EnemyChargeRing");

  if (squadHitShotCooldown_ > 0.0f) {
    squadHitShotCooldown_ -= deltaTime;
    if (squadHitShotCooldown_ <= 0.0f) {
      squadHitShotCooldown_ = 0.0f;
      squadHitShotCount_ = 0;
      squadFiredHitShotCount_ = 0;
    }
  }

  for (auto& enemy : enemies_) {
    if (enemy.isDead) {
      if (enemy.deathTimer < 1.0f) {
        enemy.deathTimer += deltaTime * 2.0f;
        if (enemy.deathTimer > 1.0f) {
          enemy.deathTimer = 1.0f;
        }
        if (enemy.model) {
          enemy.model->SetDissolveParams(1, enemy.deathTimer, 0.05f, Vector3(1.0f, 0.4f, 0.3f));
        }
        enemy.object->Update(mainViewIndex, camera);
      }
      continue;
    }

    float distToBattle = enemy.distance - railProgress;
    bool isApproachingOrActive = (enemy.isActive || (distToBattle <= 5.0f && distToBattle >= 0.0f));

    if (isApproachingOrActive) {
      enemy.slideTimer += deltaTime;
      float activeSlideTime = enemy.slideTimer - enemy.slideDelay;
      float moveT = 0.0f;
      if (activeSlideTime > 0.0f) {
        moveT = activeSlideTime / enemy.slideDuration;
        if (moveT > 1.0f) moveT = 1.0f;
      }
      float easedT = 1.0f - std::pow(1.0f - moveT, 3.0f);
      Vector3 currentPos{
          enemy.initialOffsetPosition.x + (enemy.basePosition.x - enemy.initialOffsetPosition.x) * easedT,
          enemy.initialOffsetPosition.y + (enemy.basePosition.y - enemy.initialOffsetPosition.y) * easedT,
          enemy.initialOffsetPosition.z + (enemy.basePosition.z - enemy.initialOffsetPosition.z) * easedT
      };
      enemy.object->SetTranslate(currentPos);
      enemy.spawnTimer = moveT;
    } else {
      enemy.object->SetTranslate(enemy.initialOffsetPosition);
      enemy.spawnTimer = 0.0f;
    }

    if (!enemy.isActive) {
      enemy.object->Update(mainViewIndex, camera);
      continue;
    }

    if (enemy.spawnTimer >= 1.0f) {
      float spawnInterval = enemy.behavior->GetSpawnInterval(kProjectileSpawnInterval, kEngineerSpawnInterval);
      float chargeStart = spawnInterval - kChargeDuration;

      enemy.behavior->PrepareFirstShotIfNeeded(chargeStart, enemy.shootTimer, enemy.isNextHitShot, enemy.hasFiredEliteFirstShot);

      if (enemy.isNextHitShot && !enemy.isHitShotReserved && enemy.shootTimer >= chargeStart) {
        if (squadHitShotCooldown_ <= 0.0f && squadHitShotCount_ < kMaxSquadHitShots) {
          squadHitShotCount_++;
          enemy.isHitShotReserved = true;
        } else {
          enemy.isNextHitShot = false;
          enemy.isHitShotReserved = false;
        }
      }

      if (enemy.isNextHitShot && enemy.isHitShotReserved && enemy.shootTimer >= chargeStart && enemy.shootTimer < spawnInterval) {
        float t = std::clamp((enemy.shootTimer - chargeStart) / kChargeDuration, 0.0f, 1.0f);
        float shrinkRate = 1.0f - t;
        Vector3 ringScale = {shrinkRate * 2.5f, shrinkRate * 1.0f, shrinkRate * 2.5f};

        Vector3 startPos = enemy.object->GetTranslate();
        Vector3 dir = Normalize(Subtract(playerPos, startPos));
        Vector3 muzzlePos = Add(startPos, Multiply(1.2f, dir));
        Vector3 ringRot = CalculateRingRotation(dir);

        Vector4 ringColor = enemy.behavior->GetChargeRingColor(enemy.hitShotCount);
        ParticleManager::GetInstance()->EmitCustom("EnemyChargeRing", muzzlePos, ringRot, ringScale, ringColor, 1.0f);
      }

      enemy.shootTimer += 1.0f;
      if (enemy.shootTimer >= spawnInterval) {
        enemy.shootTimer = 0.0f;
        if (enemy.behavior->ShouldMarkFirstShotFired() && !enemy.hasFiredEliteFirstShot) {
          enemy.hasFiredEliteFirstShot = true;
        }
        Vector3 startPos = enemy.object->GetTranslate();
        enemy.shootCount++;

        enemy.isNextHitShot = (enemy.isNextHitShot && enemy.isHitShotReserved);
        enemy.isHitShotReserved = false;
        if (enemy.isNextHitShot) {
          squadFiredHitShotCount_++;
          if (squadFiredHitShotCount_ >= kMaxSquadHitShots) {
            squadHitShotCooldown_ = kSquadHitShotCooldownDuration;
          }
        }
        bool isHitShot = enemy.isNextHitShot;

        Vector3 targetPos = playerPos;
        EnemyProjectile::Type pType = EnemyProjectile::Type::Normal;

        if (isHitShot) {
          enemy.hitShotCount++;
          pType = enemy.behavior->SelectHitProjectileType(enemy.hitShotCount);
        } else {
          float minOffset = 2.5f;
          float maxOffset = (missShotSpread_ > minOffset) ? missShotSpread_ : minOffset + 1.5f;
          float angle = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
          float offsetDist = minOffset + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * (maxOffset - minOffset);

          targetPos.x += std::cos(angle) * offsetDist;
          float offsetY = std::sin(angle) * offsetDist;
          if (offsetY < 0.0f) {
            offsetY -= 1.0f;
          }
          targetPos.y += offsetY;

          pType = enemy.behavior->SelectMissProjectileType();
        }

        Vector3 dir = Normalize(Subtract(targetPos, startPos));
        float bulletSpeed = speed_;
        if (pType == EnemyProjectile::Type::Blast) {
          bulletSpeed = engineerProjectileSpeed_;
        } else if (pType == EnemyProjectile::Type::Jamming) {
          bulletSpeed = jammingProjectileSpeed_;
        }
        Vector3 velocity = Multiply(bulletSpeed, dir);

        auto newProjectile = AcquireProjectile(pType, startPos, velocity);
        newProjectile->SetHitShot(isHitShot);
        projectiles_.push_back(std::move(newProjectile));

        float nextHitRate = enemy.behavior->GetHitShotRate(hitShotRate_);
        float nextRand = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        enemy.isNextHitShot = (nextRand < nextHitRate);
      }
    }

    enemy.object->Update(mainViewIndex, camera);
  }

  for (auto& p : projectiles_) {
    p->Update();
    p->Update(mainViewIndex, camera);
  }
}

ShootingWaveManager::PlayerHitReport ShootingWaveManager::ResolveProjectileHits(
    bool isCovering, bool isInvincible, bool isForceInvincible,
    const Vector3& railTargetPos, const Vector3& camPos,
    int playerHp, int directDamage) {
  PlayerHitReport report;

  if (isCovering) {
    for (auto& p : projectiles_) {
      if (p->IsDead()) continue;
      Vector3 toProjectile = Subtract(p->GetPosition(), railTargetPos);
      if (Dot(toProjectile, p->GetVelocity()) >= 0.0f) {
        if (p->IsExplosive() && p->IsHitShot() && !isForceInvincible) {
          report.coverBlastHits++;
        }
        p->Kill();
      }
    }

    if (!isInvincible) {
      for (auto& p : projectiles_) {
        if (p->IsDead() || !p->IsHitShot()) continue;
        float dist = Length(Subtract(p->GetPosition(), camPos));
        if (dist < 1.0f) {
          if (p->IsExplosive() && playerHp > directDamage) {
            report.triggerDirectBlastFx = true;
          } else {
            report.triggerDirectNormalFx = true;
          }

          if (p->IsJamming()) {
            report.jammingHits++;
            p->Kill();
            continue;
          }

          p->Kill();
          if (p->GetType() == EnemyProjectile::Type::Blast) {
            report.directBlastHits++;
          } else {
            report.directNormalHits++;
          }
        }
      }
    }
  } else {
    for (auto& p : projectiles_) {
      if (p->IsDead()) continue;
      if (p->IsHitShot()) {
        float dist = Length(Subtract(p->GetPosition(), camPos));
        if (dist < 1.0f) {
          if (!isInvincible) {
            if (p->IsExplosive() && playerHp > directDamage) {
              report.triggerDirectBlastFx = true;
            } else {
              report.triggerDirectNormalFx = true;
            }
          }

          if (p->IsJamming()) {
            if (!isInvincible) {
              report.jammingHits++;
            }
            p->Kill();
            continue;
          }

          p->Kill();
          if (!isInvincible) {
            if (p->GetType() == EnemyProjectile::Type::Blast) {
              report.directBlastHits++;
            } else {
              report.directNormalHits++;
            }
          }
        }
      }

      Vector3 toProj = Subtract(p->GetPosition(), railTargetPos);
      if (Dot(toProj, p->GetVelocity()) >= 0.0f && Length(toProj) > 5.0f) {
        p->Kill();
      }
    }
  }

  return report;
}

void ShootingWaveManager::RemoveDeadProjectiles() {
  size_t writeIdx = 0;
  for (size_t i = 0; i < projectiles_.size(); ++i) {
    if (projectiles_[i] && projectiles_[i]->IsDead()) {
      ReleaseProjectileToPool(std::move(projectiles_[i]));
    } else {
      if (writeIdx != i) {
        projectiles_[writeIdx] = std::move(projectiles_[i]);
      }
      ++writeIdx;
    }
  }
  projectiles_.resize(writeIdx);
}

void ShootingWaveManager::ProcessPlayerShot(
    const Vector2& mousePos, float clientWidth, float clientHeight,
    Camera* camera, float railProgress, ShootingEffectManager* effectManager) {
  if (!camera) return;

  float x = mousePos.x / clientWidth * 2.0f - 1.0f;
  float y = 1.0f - mousePos.y / clientHeight * 2.0f;
  Matrix4x4 invVP = Inverse(camera->GetViewProjectionMatrix());
  Vector3 nearPos = TransformPoint({x, y, 0.0f}, invVP);
  Vector3 farPos = TransformPoint({x, y, 1.0f}, invVP);
  Vector3 rayDir = Normalize(Subtract(farPos, nearPos));

  if (swarmManager_) {
    hasPendingSwarmAttack_ = true;
    pendingSwarmAttack_.rayOrigin = {nearPos.x, nearPos.y, nearPos.z};
    pendingSwarmAttack_.rayDirection = {rayDir.x, rayDir.y, rayDir.z};
    pendingSwarmAttack_.rayRadius = 2.0f;
    pendingSwarmAttack_.damage = 10.0f;
  }

  float closestT = (std::numeric_limits<float>::max)();
  enum class HitTargetType { None, Enemy, BlastProjectile };
  HitTargetType hitType = HitTargetType::None;
  EnemyInfo* hitEnemy = nullptr;
  EnemyProjectile* hitProjectile = nullptr;

  for (auto& p : projectiles_) {
    if (p->IsDead() || !p->IsExplosive()) continue;
    Vector3 projPos = p->GetPosition();
    Vector3 toProj = Subtract(projPos, nearPos);
    float t = Dot(toProj, rayDir);
    if (t > 0.0f && t < closestT) {
      Vector3 closestPoint = Add(nearPos, Multiply(t, rayDir));
      if (Length(Subtract(projPos, closestPoint)) < p->GetRadius() * 1.2f) {
        closestT = t;
        hitType = HitTargetType::BlastProjectile;
        hitProjectile = p.get();
      }
    }
  }

  for (auto& enemy : enemies_) {
    if (enemy.isDead) continue;
    float distToBattle = enemy.distance - railProgress;
    if (!enemy.isActive && !(distToBattle <= 5.0f && distToBattle >= 0.0f)) continue;

    Vector3 enemyPos = enemy.object->GetTranslate();
    Vector3 toEnemy = Subtract(enemyPos, nearPos);
    float t = Dot(toEnemy, rayDir);
    if (t > 0.0f && t < closestT) {
      Vector3 closestPoint = Add(nearPos, Multiply(t, rayDir));
      if (Length(Subtract(enemyPos, closestPoint)) < 1.0f) {
        closestT = t;
        hitType = HitTargetType::Enemy;
        hitEnemy = &enemy;
      }
    }
  }

  if (hitType == HitTargetType::BlastProjectile && hitProjectile) {
    Vector3 pPos = hitProjectile->GetPosition();
    if (effectManager) effectManager->EmitPlayerHit(pPos);
    bool isDestroyed = hitProjectile->ApplyDamage(1);
    if (isDestroyed) {
      if (effectManager) effectManager->EmitSpark(pPos, 24);
      NotifyScoreEarned(5);
    } else {
      if (effectManager) effectManager->EmitAmmoSpark(pPos, 6);
    }
  } else if (hitType == HitTargetType::Enemy && hitEnemy) {
    Vector3 enemyPos = hitEnemy->object->GetTranslate();
    if (effectManager) effectManager->EmitPlayerHit(enemyPos);
    hitEnemy->hp--;

    if (hitEnemy->hp > 0) {
      if (effectManager) effectManager->EmitAmmoSpark(enemyPos, 10);
    } else {
      hitEnemy->hp = 0;
      hitEnemy->isDead = true;
      hitEnemy->isActive = false;
      hitEnemy->isHpGaugeVisible = false;
      hitEnemy->deathTimer = 0.0f;
      if (hitEnemy->behavior->DropsMachineGunAmmoOnDeath()) {
        NotifyEliteEnemyDefeated(enemyPos);
      }
      if (hitEnemy->isHitShotReserved) {
        hitEnemy->isHitShotReserved = false;
        if (squadHitShotCount_ > squadFiredHitShotCount_) {
          squadHitShotCount_--;
        }
      }
      if (hitEnemy->model) {
        hitEnemy->model->SetDissolveParams(1, 0.0f, 0.05f, Vector3(1.0f, 0.4f, 0.3f));
      }
      NotifyScoreEarned(10);
      if (effectManager) effectManager->EmitCylinder(enemyPos, 32);
    }
  } else if (hitType == HitTargetType::None) {
    bool hitSwarm = (swarmManager_ && swarmManager_->IsActive() && swarmManager_->IsRayHitting(nearPos, rayDir));
    if (!hitSwarm && rayDir.y < -0.01f && nearPos.y > 0.0f) {
      float tGround = -nearPos.y / rayDir.y;
      if (tGround > 0.0f && tGround < 120.0f) {
        Vector3 groundHitPos = Add(nearPos, Multiply(tGround, rayDir));
        groundHitPos.y += 0.05f;
        if (effectManager) effectManager->EmitPlayerHit(groundHitPos);
      }
    }
  }
}

bool ShootingWaveManager::UpdateAutoClearBossAttack(float deltaTime, Camera* camera, float& autoClearAttackTimer, Vector3& outTargetPos) {
  if (!swarmManager_ || !swarmManager_->IsBossMode() || !camera) return false;

  if (swarmManager_->GetAutoAimTarget(outTargetPos)) {
    autoClearAttackTimer += deltaTime;
    float attackInterval = (swarmManager_->GetBossPhase() == 3) ? 0.03f : 0.0f;
    if (autoClearAttackTimer >= attackInterval) {
      autoClearAttackTimer = 0.0f;
      Vector3 camPos = camera->GetTranslate();
      Vector3 toTarget = Subtract(outTargetPos, camPos);
      if (Length(toTarget) > 0.0001f) {
        Vector3 rayDir = Normalize(toTarget);
        hasPendingSwarmAttack_ = true;
        pendingSwarmAttack_.rayOrigin = {camPos.x, camPos.y, camPos.z};
        pendingSwarmAttack_.rayDirection = {rayDir.x, rayDir.y, rayDir.z};
        pendingSwarmAttack_.rayRadius = 2.5f;
        pendingSwarmAttack_.damage = 10.0f;
      }
    }
    return true;
  }
  return false;
}

ShootingWaveManager::SwarmStepResult ShootingWaveManager::UpdateSwarmSimulation(
    float deltaTime, const Vector3& playerPos, Camera* camera,
    bool isCovering, ShootingEffectManager* effectManager) {
  SwarmStepResult res;
  if (!swarmManager_) return res;

  if (hasPendingSwarmAttack_) {
    swarmManager_->Update(deltaTime, playerPos, camera, &pendingSwarmAttack_, isCovering);
    hasPendingSwarmAttack_ = false;
  } else {
    swarmManager_->Update(deltaTime, playerPos, camera, nullptr, isCovering);
  }

  const auto& swarmEvents = swarmManager_->GetFrameEvents();
  if (effectManager) {
    for (const auto& pos : swarmEvents.chargeStartPositions) {
      effectManager->EmitBossCharge(pos);
    }
    for (const auto& pos : swarmEvents.hitPositions) {
      effectManager->EmitPlayerHit(pos);
      effectManager->EmitAmmoSpark(pos, 10);
    }
    for (const auto& pos : swarmEvents.destroyedPositions) {
      effectManager->EmitSpark(pos, 24);
      effectManager->EmitCylinder(pos, 8);
    }
    for (const auto& pos : swarmEvents.selfDestructPositions) {
      effectManager->EmitBossAttack(pos);
    }
  }

  NotifyScoreEarned(swarmEvents.dronesKilledByPlayer * 5);
  res.groupBlastHits = swarmEvents.playerGroupBlastHits;
  res.directHits = swarmEvents.playerDirectHits;
  res.bossDefeated = swarmEvents.bossDefeated;
  return res;
}

void ShootingWaveManager::UpdateHpGauges(Camera* camera, float clientWidth, float clientHeight,
                                         float railProgress, bool isSwarmBattle) {
  if (!camera) return;
  const Matrix4x4& vp = camera->GetViewProjectionMatrix();
  const Vector3& camPos = camera->GetTranslate();
  const Vector3& camRot = camera->GetRotate();
  Vector3 camForward = {
      std::cos(camRot.x) * std::sin(camRot.y),
      -std::sin(camRot.x),
      std::cos(camRot.x) * std::cos(camRot.y)
  };

  for (auto& enemy : enemies_) {
    enemy.isHpGaugeVisible = false;
    if (enemy.isDead || enemy.maxHp <= 1 || enemy.hp >= enemy.maxHp) {
      continue;
    }

    float distToBattle = enemy.distance - railProgress;
    if (!enemy.isActive && !(distToBattle <= 5.0f && distToBattle >= 0.0f)) {
      continue;
    }

    Vector3 headWorldPos = Add(enemy.object->GetTranslate(), {0.0f, 1.6f, 0.0f});
    Vector3 toHead = Subtract(headWorldPos, camPos);
    if (Dot(toHead, camForward) <= 0.1f) continue;

    Vector3 ndc = TransformPoint(headWorldPos, vp);
    if (ndc.z < 0.0f || ndc.z > 1.0f) continue;

    float screenX = (ndc.x * 0.5f + 0.5f) * clientWidth;
    float screenY = (1.0f - (ndc.y * 0.5f + 0.5f)) * clientHeight;

    float targetRatio = (std::max)(0.0f, static_cast<float>(enemy.hp) / static_cast<float>(enemy.maxHp));
    if (enemy.damageBarRatio > targetRatio) {
      enemy.damageBarRatio -= (enemy.damageBarRatio - targetRatio) * 0.10f;
      if (enemy.damageBarRatio - targetRatio < 0.005f) {
        enemy.damageBarRatio = targetRatio;
      }
    } else {
      enemy.damageBarRatio = targetRatio;
    }

    const float kBarFullWidth = 74.0f;
    const float kBarHeight = 8.0f;
    float barLeftX = screenX - kBarFullWidth * 0.5f;

    if (enemy.hpBgSprite) {
      enemy.hpBgSprite->SetTranslate({barLeftX, screenY});
      enemy.hpBgSprite->SetScale({kBarFullWidth, kBarHeight});
      enemy.hpBgSprite->Update();
    }
    if (enemy.hpDamageSprite) {
      enemy.hpDamageSprite->SetTranslate({barLeftX, screenY});
      enemy.hpDamageSprite->SetScale({kBarFullWidth * enemy.damageBarRatio, kBarHeight});
      enemy.hpDamageSprite->Update();
    }
    if (enemy.hpFillSprite) {
      enemy.hpFillSprite->SetTranslate({barLeftX, screenY});
      enemy.hpFillSprite->SetScale({kBarFullWidth * targetRatio, kBarHeight});
      enemy.hpFillSprite->SetColor(targetRatio <= 0.35f ? Vector4{1.0f, 0.35f, 0.1f, 1.0f} : Vector4{1.0f, 0.85f, 0.1f, 1.0f});
      enemy.hpFillSprite->Update();
    }
    if (enemy.hpFrameSprite) {
      enemy.hpFrameSprite->SetTranslate({screenX, screenY});
      enemy.hpFrameSprite->SetScale({84.0f, 16.0f});
      enemy.hpFrameSprite->Update();
    }

    enemy.isHpGaugeVisible = true;
  }

  if (swarmManager_ && swarmManager_->IsActive()) {
    swarmManager_->UpdateHpGauges(camera, clientWidth, clientHeight);

    if (isSwarmBattle && swarmManager_->IsBossMode()) {
      float maxHp = static_cast<float>((std::max)(1u, swarmManager_->GetMaxBossHp()));
      float curHp = static_cast<float>(swarmManager_->GetAliveDroneCount());
      float targetRatio = std::clamp(curHp / maxHp, 0.0f, 1.0f);
      if (bossDamageBarRatio_ > targetRatio) {
        bossDamageBarRatio_ -= (bossDamageBarRatio_ - targetRatio) * 0.10f;
        if (bossDamageBarRatio_ - targetRatio < 0.003f) {
          bossDamageBarRatio_ = targetRatio;
        }
      } else {
        bossDamageBarRatio_ = targetRatio;
      }

      const float kBossBarWidth = 360.0f;
      const float kBossBarHeight = 12.0f;
      const float kBossBarCenterX = 640.0f;
      const float kBossBarCenterY = 128.0f;
      float barLeftX = kBossBarCenterX - kBossBarWidth * 0.5f;

      if (bossHpBgSprite_) {
        bossHpBgSprite_->SetTranslate({barLeftX, kBossBarCenterY});
        bossHpBgSprite_->SetScale({kBossBarWidth, kBossBarHeight});
        bossHpBgSprite_->Update();
      }
      if (bossHpDamageSprite_) {
        bossHpDamageSprite_->SetTranslate({barLeftX, kBossBarCenterY});
        bossHpDamageSprite_->SetScale({kBossBarWidth * bossDamageBarRatio_, kBossBarHeight});
        bossHpDamageSprite_->Update();
      }
      if (bossHpFillSprite_) {
        bossHpFillSprite_->SetTranslate({barLeftX, kBossBarCenterY});
        bossHpFillSprite_->SetScale({kBossBarWidth * targetRatio, kBossBarHeight});
        int bPhase = swarmManager_->GetBossPhase();
        if (bPhase == 3) {
          bossHpFillSprite_->SetColor({1.0f, 0.2f, 0.15f, 1.0f});
        } else if (bPhase == 2) {
          bossHpFillSprite_->SetColor({1.0f, 0.75f, 0.1f, 1.0f});
        } else {
          bossHpFillSprite_->SetColor({0.2f, 0.95f, 0.7f, 1.0f});
        }
        bossHpFillSprite_->Update();
      }
      if (bossHpFrameSprite_) {
        bossHpFrameSprite_->SetTranslate({kBossBarCenterX, kBossBarCenterY});
        bossHpFrameSprite_->SetScale({kBossBarWidth + 20.0f, kBossBarHeight + 10.0f});
        bossHpFrameSprite_->Update();
      }
    }
  }

  for (auto& p : projectiles_) {
    if (p) {
      p->UpdateHpGauge(camera, clientWidth, clientHeight);
    }
  }
}

bool ShootingWaveManager::CheckAiming(const Vector3& nearPos, const Vector3& rayDir, float railProgress) const {
  for (const auto& p : projectiles_) {
    if (p->IsDead() || !p->IsExplosive()) continue;
    Vector3 projPos = p->GetPosition();
    Vector3 toProj = Subtract(projPos, nearPos);
    float t = Dot(toProj, rayDir);
    if (t > 0.0f) {
      Vector3 closestPoint = Add(nearPos, Multiply(t, rayDir));
      if (Length(Subtract(projPos, closestPoint)) < p->GetRadius() * 1.2f) {
        return true;
      }
    }
  }

  if (swarmManager_ && swarmManager_->IsActive()) {
    if (swarmManager_->IsRayHitting(nearPos, rayDir)) {
      return true;
    }
  }

  for (const auto& enemy : enemies_) {
    if (enemy.isDead) continue;
    float distToBattle = enemy.distance - railProgress;
    if (!enemy.isActive && !(distToBattle <= 5.0f && distToBattle >= 0.0f)) continue;
    Vector3 enemyPos = enemy.object->GetTranslate();
    Vector3 toEnemy = Subtract(enemyPos, nearPos);
    float t = Dot(toEnemy, rayDir);
    if (t > 0.0f) {
      Vector3 closestPoint = Add(nearPos, Multiply(t, rayDir));
      if (Length(Subtract(enemyPos, closestPoint)) < 1.0f) {
        return true;
      }
    }
  }

  return false;
}

bool ShootingWaveManager::AreAllEnemiesDead() const {
  for (const auto& enemy : enemies_) {
    if (!enemy.isDead) return false;
  }
  return true;
}

float ShootingWaveManager::GetNextBattleDistance(float currentProgress, float maxProgress) const {
  float nextBattleDist = maxProgress;
  for (const auto& enemy : enemies_) {
    if (!enemy.isDead && enemy.distance > currentProgress && enemy.distance < nextBattleDist) {
      nextBattleDist = enemy.distance;
    }
  }
  return nextBattleDist;
}

void ShootingWaveManager::RespawnIntroSwarm(uint32_t count, const Vector3& center, float radius) {
  if (swarmManager_) {
    swarmManager_->SpawnDrones(count, center, radius);
    swarmManager_->SetAttackDistance(0.0f);
  }
}

std::vector<bool> ShootingWaveManager::ComputeSectionCleared(const std::vector<float>& sectionProgresses) const {
  std::vector<bool> cleared(sectionProgresses.size(), true);
  for (size_t i = 0; i < sectionProgresses.size(); ++i) {
    float waveDist = sectionProgresses[i];
    for (const auto& enemy : enemies_) {
      if (std::abs(enemy.distance - waveDist) < 0.1f && !enemy.isDead) {
        cleared[i] = false;
        break;
      }
    }
  }
  return cleared;
}

void ShootingWaveManager::PopulateDemoAIContext(ShootingDemoAI::Context& ctx, float railProgress, const Vector3& camPos, bool isSwarmBattle) const {
  if (!isSwarmBattle) {
    float minDistToCamera = 99999.0f;
    for (const auto& enemy : enemies_) {
      if (enemy.isActive && !enemy.isDead && enemy.object) {
        float dist = std::abs(enemy.distance - railProgress);
        if (dist < minDistToCamera) {
          minDistToCamera = dist;
          ctx.hasTargetEnemy = true;
          ctx.targetEnemyPos = enemy.object->GetTranslate();
        }
      }
    }

    constexpr float kNormalDodgeThreshold = 12.0f;
    for (const auto& p : projectiles_) {
      if (!p || p->IsDead() || !p->IsHitShot()) continue;
      float dist = Length(Subtract(p->GetPosition(), camPos));
      if (p->IsExplosive()) {
        if (dist < ctx.minBlastProjDist) {
          ctx.minBlastProjDist = dist;
          ctx.hasTargetBlastProj = true;
          ctx.targetBlastProjPos = p->GetPosition();
        }
      } else {
        if (dist < kNormalDodgeThreshold) {
          ctx.hasIncomingNormalProj = true;
        }
      }
    }
  } else {
    ctx.bossPhase = swarmManager_ ? swarmManager_->GetBossPhase() : 1;
    if (swarmManager_) {
      ctx.hasSwarmTarget = swarmManager_->GetAutoAimTarget(ctx.swarmTargetPos);
    }
  }
}

bool ShootingWaveManager::IsBossDefeated() const {
  return swarmManager_ && swarmManager_->IsBossDefeated();
}

bool ShootingWaveManager::IsBossMode() const {
  return swarmManager_ && swarmManager_->IsBossMode();
}

bool ShootingWaveManager::IsSwarmActive() const {
  return swarmManager_ && swarmManager_->IsActive();
}

uint32_t ShootingWaveManager::GetAliveDroneCount() const {
  return swarmManager_ ? swarmManager_->GetAliveDroneCount() : 0;
}

bool ShootingWaveManager::GetBossCameraTarget(float& outYaw, float& outPitch) const {
  if (swarmManager_ && swarmManager_->IsBossMode()) {
    outYaw = swarmManager_->GetTargetCameraYaw();
    outPitch = swarmManager_->GetTargetCameraPitch();
    return true;
  }
  return false;
}

void ShootingWaveManager::ResetForJumpToSection(float targetProgress, int mainViewIndex, Camera* camera, int& outClearedScore) {
  outClearedScore = 0;
  squadHitShotCount_ = 0;
  squadFiredHitShotCount_ = 0;
  squadHitShotCooldown_ = 0.0f;
  if (swarmManager_) swarmManager_->ClearDrones();
  if (ambientSwarmManager_) ambientSwarmManager_->ClearDrones();
  projectiles_.clear();

  for (auto& enemy : enemies_) {
    enemy.shootCount = 0;
    enemy.hitShotCount = 0;
    enemy.isHitShotReserved = false;
    enemy.hasFiredEliteFirstShot = false;
    ApplyEnemyType(enemy, enemy.behavior->GetType());
    if (enemy.distance < targetProgress) {
      enemy.isDead = true;
      enemy.isActive = false;
      enemy.shootTimer = 0.0f;
      enemy.spawnTimer = 1.0f;
      outClearedScore += 10;
      if (enemy.model) {
        enemy.model->SetDissolveParams(0, 0.0f, 0.05f, Vector3(1.0f, 0.4f, 0.3f));
      }
    } else {
      enemy.isDead = false;
      enemy.isActive = (enemy.distance == targetProgress);
      enemy.shootTimer = 0.0f;
      enemy.slideTimer = (enemy.distance == targetProgress) ? 10.0f : 0.0f;
      enemy.deathTimer = 0.0f;
      float rate = enemy.behavior->GetHitShotRate(hitShotRate_);
      enemy.isNextHitShot = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < rate);
      enemy.object->SetTranslate(enemy.distance == targetProgress ? enemy.basePosition : enemy.initialOffsetPosition);
      enemy.object->SetRotation(enemy.baseRotation);
      if (enemy.model) {
        enemy.model->SetDissolveParams(0, 0.0f, 0.05f, Vector3(1.0f, 0.4f, 0.3f));
      }
    }
    enemy.object->Update(mainViewIndex, camera);
  }
}

void ShootingWaveManager::ResetForGameReset(int mainViewIndex, Camera* camera) {
  projectiles_.clear();
  squadHitShotCount_ = 0;
  squadFiredHitShotCount_ = 0;
  squadHitShotCooldown_ = 0.0f;
  for (auto& enemy : enemies_) {
    enemy.isDead = false;
    enemy.isActive = false;
    enemy.shootTimer = 0.0f;
    enemy.shootCount = 0;
    enemy.hitShotCount = 0;
    enemy.isHitShotReserved = false;
    enemy.spawnTimer = 0.0f;
    enemy.slideTimer = 0.0f;
    enemy.deathTimer = 0.0f;
    ApplyEnemyType(enemy, enemy.behavior->GetType());
    float rate = enemy.behavior->GetHitShotRate(hitShotRate_);
    enemy.isNextHitShot = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < rate);
    enemy.object->SetTranslate(enemy.initialOffsetPosition);
    enemy.object->SetRotation(enemy.baseRotation);
    if (enemy.model) {
      enemy.model->SetDissolveParams(0, 0.0f, 0.05f, Vector3(1.0f, 0.4f, 0.3f));
    }
    enemy.object->Update(mainViewIndex, camera);
  }
  if (swarmManager_) {
    swarmManager_->ClearDrones();
  }
}

void ShootingWaveManager::JumpToBossBattle(const Vector3& basePos, float baseYaw) {
  for (auto& enemy : enemies_) {
    enemy.isDead = true;
    enemy.isActive = false;
    enemy.isHpGaugeVisible = false;
    enemy.deathTimer = 1.0f;
  }
  projectiles_.clear();
  if (swarmManager_) {
    swarmManager_->ClearDrones();
    swarmManager_->SetAttackDistance(20.0f);
    swarmManager_->StartBossBattle(kSpawnSwarmCount, basePos, baseYaw);
  }
  if (ambientSwarmManager_) {
    ambientSwarmManager_->ClearDrones();
  }
  swarmBattleTimer_ = 0.0f;
  bossDamageBarRatio_ = 1.0f;
}

void ShootingWaveManager::KillAllForAutoClear() {
  if (swarmManager_) {
    swarmManager_->ClearDrones();
    swarmManager_->SetAttackDistance(20.0f);
  }
  if (ambientSwarmManager_) {
    ambientSwarmManager_->ClearDrones();
  }
  for (auto& enemy : enemies_) {
    enemy.isDead = true;
    enemy.isActive = false;
    enemy.isHpGaugeVisible = false;
    enemy.isHitShotReserved = false;
    enemy.deathTimer = 1.0f;
  }
  projectiles_.clear();
}

void ShootingWaveManager::DrawEnemiesAndProjectiles(float railProgress, bool drawEnemies, Camera* camera) {
  if (drawEnemies) {
    for (auto& enemy : enemies_) {
      if (!enemy.isDead) {
        float distToBattle = enemy.distance - railProgress;
        if (enemy.isActive || (distToBattle <= 5.0f && distToBattle >= 0.0f)) {
          enemy.object->Draw(0);
        }
      } else if (enemy.deathTimer < 1.0f) {
        enemy.object->Draw(0);
      }
    }
  }

  for (auto& p : projectiles_) {
    p->Draw(0);
  }

  if (ambientSwarmManager_) {
    ambientSwarmManager_->Draw(camera);
  }
  if (swarmManager_) {
    swarmManager_->Draw(camera);
  }
}

void ShootingWaveManager::DrawHpGauges(bool isSwarmBattle) {
  for (auto& enemy : enemies_) {
    if (enemy.isHpGaugeVisible) {
      if (enemy.hpBgSprite) enemy.hpBgSprite->Draw();
      if (enemy.hpDamageSprite) enemy.hpDamageSprite->Draw();
      if (enemy.hpFillSprite) enemy.hpFillSprite->Draw();
      if (enemy.hpFrameSprite) enemy.hpFrameSprite->Draw();
    }
  }

  if (swarmManager_ && swarmManager_->IsActive()) {
    swarmManager_->DrawHpGauges();
    if (isSwarmBattle && swarmManager_->IsBossMode()) {
      if (bossHpBgSprite_) bossHpBgSprite_->Draw();
      if (bossHpDamageSprite_) bossHpDamageSprite_->Draw();
      if (bossHpFillSprite_) bossHpFillSprite_->Draw();
      if (bossHpFrameSprite_) bossHpFrameSprite_->Draw();
    }
  }

  for (auto& p : projectiles_) {
    if (p) {
      p->DrawHpGauge();
    }
  }
}

Vector3 ShootingWaveManager::CalculateRingRotation(const Vector3& direction) const {
  Vector3 forward = direction;
  Vector3 up = {0.0f, 1.0f, 0.0f};
  if (std::abs(forward.y) > 0.99f) {
    up = {0.0f, 0.0f, 1.0f};
  }
  Vector3 right = Normalize(Cross(up, forward));
  up = Cross(forward, right);

  // Cylinderの円筒軸（ローカルY軸）を射出方向(forward)に向ける回転行列
  Matrix4x4 m = MathGenerators::MakeIdentity4x4();
  m.m[0][0] = right.x;   m.m[0][1] = right.y;   m.m[0][2] = right.z;
  m.m[1][0] = forward.x; m.m[1][1] = forward.y; m.m[1][2] = forward.z;
  m.m[2][0] = up.x;      m.m[2][1] = up.y;      m.m[2][2] = up.z;

  float sy = std::clamp(-m.m[0][2], -1.0f, 1.0f);
  float yaw = std::asin(sy);
  float pitch = 0.0f;
  float roll = 0.0f;
  if (std::cos(yaw) > 0.0001f) {
    pitch = std::atan2(m.m[1][2], m.m[2][2]);
    roll = std::atan2(m.m[0][1], m.m[0][0]);
  } else {
    pitch = std::atan2(-m.m[2][1], m.m[1][1]);
    roll = 0.0f;
  }
  return {pitch, yaw, roll};
}

Vector3 ShootingWaveManager::CalculateBossEntranceAnchor(float t, const Vector3& basePos, float baseYaw) const {
  float clampedT = std::clamp(t, 0.0f, 1.0f);

  static constexpr Vector3 kWaypoints[] = {
      { -34.0f, 24.0f, 26.0f },
      {   0.0f,  7.0f, 20.0f },
      {   0.0f,  0.8f,  9.0f },
      {   0.0f,  2.5f, 14.0f },
      {   0.0f,  9.5f, 20.0f },
      {   0.0f,  4.5f, 22.0f }
  };
  constexpr int kSegmentCount = 5;

  float scaled = clampedT * static_cast<float>(kSegmentCount);
  int seg = (std::min)(kSegmentCount - 1, static_cast<int>(scaled));
  float localT = scaled - static_cast<float>(seg);
  float smoothT = localT * localT * (3.0f - 2.0f * localT);

  const Vector3& p0 = kWaypoints[seg];
  const Vector3& p1 = kWaypoints[seg + 1];
  Vector3 local = {
      p0.x + (p1.x - p0.x) * smoothT,
      p0.y + (p1.y - p0.y) * smoothT,
      p0.z + (p1.z - p0.z) * smoothT
  };

  Vector3 forward = {std::sin(baseYaw), 0.0f, std::cos(baseYaw)};
  Vector3 right = {std::cos(baseYaw), 0.0f, -std::sin(baseYaw)};
  return {
      basePos.x + right.x * local.x + forward.x * local.z,
      basePos.y + local.y,
      basePos.z + right.z * local.x + forward.z * local.z
  };
}

#ifdef USE_IMGUI
void ShootingWaveManager::UpdateObject3dImGui(Camera* camera, bool& outRequestedSwarmBattle) {
  outRequestedSwarmBattle = false;

  if (ImGui::TreeNode("Enemies")) {
    for (size_t i = 0; i < enemies_.size(); ++i) {
      auto& enemy = enemies_[i];
      const char* typeNames[] = {"Normal", "Engineer", "Elite"};
      int typeIdx = static_cast<int>(enemy.behavior->GetType());
      if (ImGui::Combo(("Enemy " + std::to_string(i) + " Type").c_str(), &typeIdx, typeNames, 3)) {
        ApplyEnemyType(enemy, static_cast<EnemyType>(typeIdx));
      }
      ImGui::Text("[%zu] Dist: %.1f, HP: %d/%d, Active: %s, Dead: %s", i, enemy.distance,
                  enemy.hp, enemy.maxHp, enemy.isActive ? "Yes" : "No", enemy.isDead ? "Yes" : "No");
      Vector3 enemyPos = enemy.object->GetTranslate();
      if (ImGui::DragFloat3(("Enemy " + std::to_string(i) + " Position").c_str(), &enemyPos.x, 0.1f)) {
        enemy.object->SetTranslate(enemyPos);
      }
      Vector3 enemyRot = enemy.object->GetRotation();
      if (ImGui::DragFloat3(("Enemy " + std::to_string(i) + " Rotation").c_str(), &enemyRot.x, 0.01f)) {
        enemy.object->SetRotation(enemyRot);
      }
      Vector3 enemyScale = enemy.object->GetScale();
      if (ImGui::DragFloat3(("Enemy " + std::to_string(i) + " Scale").c_str(), &enemyScale.x, 0.1f)) {
        enemy.object->SetScale(enemyScale);
      }
    }
    ImGui::TreePop();
  }

  ImGui::Separator();
  if (swarmManager_ && ImGui::TreeNode("Swarm System")) {
    auto settings = swarmManager_->GetSettings();
    ImGui::Text("Active: %s", swarmManager_->IsActive() ? "YES" : "NO");
    ImGui::Text("Alive / Total: %u / %u", swarmManager_->GetAliveDroneCount(), swarmManager_->GetDroneCount());
    if (swarmManager_->IsBossMode()) {
      const char* secStr = (swarmManager_->GetCurrentCameraSection() < 0) ? "Left (-1)" :
                           (swarmManager_->GetCurrentCameraSection() > 0) ? "Right (+1)" : "Center (0)";
      ImGui::Text("Boss Phase: %d | Camera Section: %s", swarmManager_->GetBossPhase(), secStr);
      ImGui::Text("No-Shoot Timer: %.2f / 2.0s | 10s Timer: %.2f / 10.0s",
                  swarmManager_->GetNoShootTimer(), swarmManager_->GetPeriodicAttackTimer());
      ImGui::Text("Evade Cooldown: %.2f s", swarmManager_->GetEvadeCooldownTimer());
    }

    if (camera && ImGui::Button("Spawn Swarm (100 drones, 20m around player)")) {
      Vector3 camPos = camera->GetTranslate();
      swarmManager_->SpawnDrones(100, {camPos.x, camPos.y, camPos.z}, 20.0f);
      swarmBattleTimer_ = 0.0f;
      outRequestedSwarmBattle = true;
    }
    if (camera && ImGui::Button("Spawn in Front of Camera (50 drones, 15m ahead)")) {
      Vector3 camPos = camera->GetTranslate();
      Vector3 spawnCenter = {camPos.x, camPos.y, camPos.z + 15.0f};
      swarmManager_->SpawnDrones(kSpawnSwarmCount, {spawnCenter.x, spawnCenter.y, spawnCenter.z}, 8.0f);
      swarmBattleTimer_ = 0.0f;
      outRequestedSwarmBattle = true;
    }

    float droneScale = swarmManager_->GetDroneScale();
    if (ImGui::SliderFloat("Drone Model Scale", &droneScale, 0.2f, 5.0f, "%.2f")) {
      swarmManager_->SetDroneScale(droneScale);
    }
    bool settingsChanged = false;
    settingsChanged |= ImGui::SliderFloat("Max Speed", &settings.maxSpeed, 1.0f, 40.0f, "%.1f");
    settingsChanged |= ImGui::SliderFloat("Charge Speed", &settings.ChargeSpeed, 5.0f, 60.0f, "%.1f");
    settingsChanged |= ImGui::SliderFloat("Attack Distance", &settings.attackDistance, 5.0f, 60.0f, "%.1f");
    ImGui::Separator();
    settingsChanged |= ImGui::SliderFloat("Neighbor Radius", &settings.neighborRadius, 2.0f, 30.0f, "%.1f");
    settingsChanged |= ImGui::SliderFloat("Separation Dist", &settings.separationDist, 0.5f, 10.0f, "%.1f");
    settingsChanged |= ImGui::SliderFloat("Separation Weight", &settings.separationWeight, 0.0f, 10.0f, "%.2f");
    settingsChanged |= ImGui::SliderFloat("Alignment Weight", &settings.alignmentWeight, 0.0f, 5.0f, "%.2f");
    settingsChanged |= ImGui::SliderFloat("Cohesion Weight", &settings.cohesionWeight, 0.0f, 5.0f, "%.2f");
    settingsChanged |= ImGui::SliderFloat("Target Weight", &settings.targetWeight, 0.0f, 5.0f, "%.2f");
    if (settingsChanged) {
      swarmManager_->SetSettings(settings);
    }
    ImGui::Separator();
    int maxRespawn = static_cast<int>(swarmManager_->GetMaxRespawnCount());
    if (ImGui::DragInt("Max Respawn Count", &maxRespawn, 10, 0, 5000)) {
      swarmManager_->SetMaxRespawnCount(static_cast<uint32_t>(maxRespawn));
    }

    if (ImGui::TreeNode("Individual Drone Status (First 15)")) {
      const auto& drones = swarmManager_->GetDronesData();
      size_t displayCount = (drones.size() < 15) ? drones.size() : 15;
      for (size_t i = 0; i < displayCount; ++i) {
        const auto& d = drones[i];
        const char* stateStr = "Cruise";
        if (d.state == 1) stateStr = "Charge";
        else if (d.state == 2) stateStr = "Dead";
        else if (d.state == 3) stateStr = "Crashing";
        else if (d.state == 4) stateStr = "Returning";

        ImGui::Text("[%02zu] State: %-8s | HP: %4.1f | Pos: (%.1f, %.1f, %.1f) | Vel: (%.1f, %.1f, %.1f)",
                    i, stateStr, d.hp, d.position.x, d.position.y, d.position.z,
                    d.velocity.x, d.velocity.y, d.velocity.z);
      }
      ImGui::TreePop();
    }
    ImGui::TreePop();
  }

  ImGui::Separator();
  if (ImGui::TreeNode("Projectiles Status")) {
    ImGui::SliderFloat("Hit Shot Ratio", &hitShotRate_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Miss Shot Spread", &missShotSpread_, 1.5f, 8.0f, "%.1f");
    ImGui::DragFloat("Normal Bullet Speed", &speed_, 0.02f, 0.1f, 3.0f);
    ImGui::DragFloat("Engineer Bullet Speed", &engineerProjectileSpeed_, 0.02f, 0.05f, 2.0f);
    ImGui::DragFloat("Jamming Bullet Speed", &jammingProjectileSpeed_, 0.02f, 0.5f, 5.0f);
    ImGui::Separator();
    ImGui::Text("Active Projectiles: %zu", projectiles_.size());
    for (size_t i = 0; i < projectiles_.size(); ++i) {
      auto& p = projectiles_[i];
      Vector3 pos = p->GetPosition();
      ImGui::Text("[%zu] Pos: (%.2f, %.2f, %.2f), Dead: %s", i, pos.x, pos.y,
                  pos.z, p->IsDead() ? "Yes" : "No");
      Vector3 projPos = p->GetPosition();
      if (ImGui::DragFloat3(("Projectile " + std::to_string(i) + " Position").c_str(), &projPos.x, 0.1f)) {
        p->SetPosition(projPos);
      }
    }
    ImGui::TreePop();
  }
}

bool ShootingWaveManager::UpdateSwarmCollisionDebugImGui(float& outDroneScale, float& outColRadius) {
  if (!swarmManager_) return false;
  outDroneScale = swarmManager_->GetDroneScale();
  float colRadiusScale = swarmManager_->GetCrashCollisionRadiusScale();
  if (ImGui::SliderFloat("Drone Model Scale", &outDroneScale, 0.05f, 1.5f, "%.3f")) {
    swarmManager_->SetDroneScale(outDroneScale);
  }
  if (ImGui::Button("Set Boss Scale (0.32)")) {
    outDroneScale = 0.32f;
    swarmManager_->SetDroneScale(outDroneScale);
  }
  if (ImGui::SliderFloat("Collision Radius Scale", &colRadiusScale, 0.5f, 5.0f, "%.2f")) {
    swarmManager_->SetCrashCollisionRadiusScale(colRadiusScale);
  }

  outColRadius = outDroneScale * colRadiusScale;
  ImGui::Text("Collision Radius: %.3f m (Diameter: %.3f m)", outColRadius, outColRadius * 2.0f);

  ImGui::Separator();
  ImGui::Text("Crashing Physics Params");
  float groundFriction = swarmManager_->GetCrashGroundFriction();
  if (ImGui::SliderFloat("Ground Friction", &groundFriction, 0.50f, 0.98f, "%.2f")) {
    swarmManager_->SetCrashGroundFriction(groundFriction);
  }
  float bounciness = swarmManager_->GetCrashBounciness();
  if (ImGui::SliderFloat("Bounciness", &bounciness, 0.0f, 0.8f, "%.2f")) {
    swarmManager_->SetCrashBounciness(bounciness);
  }
  float pushStrength = swarmManager_->GetCrashPushStrength();
  if (ImGui::SliderFloat("Push Strength", &pushStrength, 0.05f, 1.0f, "%.2f")) {
    swarmManager_->SetCrashPushStrength(pushStrength);
  }
  return true;
}
#endif // USE_IMGUI
