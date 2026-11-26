#include "ShootingEffectManager.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleManager.h"
#include <vector>

void ShootingEffectManager::Initialize() {
  InitializeSceneEffects();
  ClearAll();
}

void ShootingEffectManager::InitializeSceneEffects() {
  // JSON出力済みの全エフェクト群を読み込み
  struct EffectJsonEntry {
    const char* groupName;
    const char* filePath;
  };
  static constexpr EffectJsonEntry kJsonEffects[] = {
      {"CylinderGroup", "Resources/Effects/enemyDead_effect.json"},
      {"SparkGroup", "Resources/Effects/spark_effect.json"},
      {"ReloadCompleteGroup", "Resources/Effects/reloadComplete.json"},
      {"AmmoSparkGroup", "Resources/Effects/cartridge_effect.json"},
      {"BulletTrail_Normal", "Resources/Effects/bulletTrail_normal_effect.json"},
      {"BulletTrail_Blast", "Resources/Effects/bulletTrail_blast_effect.json"},
      {"BulletTrail_Jamming", "Resources/Effects/bulletTrail_jamming_effect.json"},
      {"EnemyChargeRing", "Resources/Effects/enemyChargeRing_effect.json"},
      {"PlayerHitRing", "Resources/Effects/playerHitRing_effect.json"},
      {"PlayerHitSpark", "Resources/Effects/playerHitSpark_effect.json"},
      {"BossChargeRing", "Resources/Effects/bossChargeRing_effect.json"},
      {"BossAttackRing", "Resources/Effects/bossAttackRing_effect.json"},
      {"BossAttackCylinder", "Resources/Effects/bossAttackCylinder_effect.json"},
      {"BossAttackSpark", "Resources/Effects/bossAttackSpark_effect.json"},
  };

  auto* particleMgr = ParticleManager::GetInstance();
  for (const auto& entry : kJsonEffects) {
    particleMgr->LoadEffectFromJson(entry.groupName, entry.filePath);
  }
}

void ShootingEffectManager::EmitPlayerHit(const Vector3& hitPos) {
  if (auto* emitter = ParticleManager::GetInstance()->GetEmitter("PlayerHitRing")) {
    emitter->SetIsPlaying(true);
    ParticleManager::GetInstance()->Emit("PlayerHitRing", hitPos, emitter->GetCount());
  }
  if (auto* emitter = ParticleManager::GetInstance()->GetEmitter("PlayerHitSpark")) {
    emitter->SetIsPlaying(true);
    ParticleManager::GetInstance()->Emit("PlayerHitSpark", hitPos, emitter->GetCount());
  }
}

void ShootingEffectManager::EmitBossCharge(const Vector3& pos) {
  if (auto* emitter = ParticleManager::GetInstance()->GetEmitter("BossChargeRing")) {
    emitter->SetIsPlaying(true);
    ParticleManager::GetInstance()->Emit("BossChargeRing", pos, emitter->GetCount());
  }
}

void ShootingEffectManager::EmitBossAttack(const Vector3& pos) {
  if (auto* emitter = ParticleManager::GetInstance()->GetEmitter("BossAttackRing")) {
    emitter->SetIsPlaying(true);
    ParticleManager::GetInstance()->Emit("BossAttackRing", pos, emitter->GetCount());
  }
  if (auto* emitter = ParticleManager::GetInstance()->GetEmitter("BossAttackCylinder")) {
    emitter->SetIsPlaying(true);
    ParticleManager::GetInstance()->Emit("BossAttackCylinder", pos, emitter->GetCount());
  }
  if (auto* emitter = ParticleManager::GetInstance()->GetEmitter("BossAttackSpark")) {
    emitter->SetIsPlaying(true);
    ParticleManager::GetInstance()->Emit("BossAttackSpark", pos, emitter->GetCount());
  }
}

void ShootingEffectManager::EmitReloadComplete(const Vector3& pos) {
  if (auto* emitter = ParticleManager::GetInstance()->GetEmitter("ReloadCompleteGroup")) {
    emitter->SetIsPlaying(true);
  }
  ParticleManager::GetInstance()->Emit("ReloadCompleteGroup", pos, 8);
}

void ShootingEffectManager::EmitAmmoSpark(const Vector3& pos, uint32_t count) {
  if (auto* emitter = ParticleManager::GetInstance()->GetEmitter("AmmoSparkGroup")) {
    emitter->SetIsPlaying(true);
  }
  ParticleManager::GetInstance()->Emit("AmmoSparkGroup", pos, count);
}

void ShootingEffectManager::EmitSpark(const Vector3& pos, uint32_t count) {
  if (auto* emitter = ParticleManager::GetInstance()->GetEmitter("SparkGroup")) {
    emitter->SetIsPlaying(true);
  }
  ParticleManager::GetInstance()->Emit("SparkGroup", pos, count);
}

void ShootingEffectManager::EmitCylinder(const Vector3& pos, uint32_t count) {
  if (auto* emitter = ParticleManager::GetInstance()->GetEmitter("CylinderGroup")) {
    emitter->SetIsPlaying(true);
  }
  ParticleManager::GetInstance()->Emit("CylinderGroup", pos, count);
}

bool ShootingEffectManager::AreClearEffectsFinished() const {
  const std::vector<std::string> clearWaitEffects = {
      "CylinderGroup", "SparkGroup", "ReloadCompleteGroup"};
  for (const auto& name : clearWaitEffects) {
    if (const auto* emitter = ParticleManager::GetInstance()->GetEmitter(name)) {
      if (emitter->IsPlaying()) {
        return false;
      }
    }
  }
  return true;
}

void ShootingEffectManager::StopClearWaitEffects() {
  const std::string clearWaitEffects[] = {
      "CylinderGroup", "SparkGroup", "ReloadCompleteGroup"};
  for (const auto& name : clearWaitEffects) {
    if (auto* emitter = ParticleManager::GetInstance()->GetEmitter(name)) {
      emitter->SetIsPlaying(false);
    }
  }
}

void ShootingEffectManager::ClearAll() {
  ParticleManager::GetInstance()->ClearAllParticles();
}
