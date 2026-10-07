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
#include "ClearScene.h"
#include <cmath>
#include <numbers>

using namespace MathUtils;
using namespace BlendMode;

ShootingScene::ShootingScene() = default;
ShootingScene::~ShootingScene() = default;

void ShootingScene::Initialize() {
  // 完成版ではマウスカーソルを非表示にする
  // ShowCursor(FALSE);

  // レベルデータのロード
  levelData_ = LevelLoader::LoadFile("testScene");

  // レールデータの抽出
  railPoints_.clear();
  if (levelData_) {
    for (const auto &obj : levelData_->objects) {
      if (obj.type == "CURVE" && obj.name == "EventRail") {
        railPoints_ = obj.bezierPoints;
        break;
      }
    }
  }
  if (railPoints_.empty()) {
    // ダミーのレールを生成 (Z方向にまっすぐ進むなど)
    LevelData::BezierControlPoint pt0, pt1;
    pt0.co = {0.0f, 2.0f, -15.0f};
    pt0.handleLeft = pt0.co;
    pt0.handleRight = pt0.co;

    pt1.co = {0.0f, 2.0f, 175.0f};
    pt1.handleLeft = pt1.co;
    pt1.handleRight = pt1.co;

    railPoints_.push_back(pt0);
    railPoints_.push_back(pt1);
  }

  // カメラの初期化
  camera_ = std::make_unique<Camera>();
  camera_->Initialize();

  if (levelData_ && !levelData_->players.empty()) {
    maxProgress_ = levelData_->players[0].distance;
    if (maxProgress_ <= 0.0f) {
      maxProgress_ = 190.0f;
    }
  } else {
    maxProgress_ = 190.0f;
  }

  cameraProgress_ = 0.0f;
  isMovementPaused_ = false;
  Vector3 startCamPos = CalculateRailPosition(cameraProgress_);
  camera_->SetTranslate(startCamPos);
  camera_->SetFovY(cameraFovY_ * (std::numbers::pi_v<float> / 180.0f));

  if (levelData_ && !levelData_->players.empty()) {
    const auto &spawn = levelData_->players[0];
    camera_->SetRotate(spawn.rotation);
    cameraBasePitch_ = spawn.rotation.x;
    cameraYaw_ = spawn.rotation.y;
    cameraBaseRoll_ = spawn.rotation.z;
  } else {
    camera_->SetRotate({0.0f, 0.0f, 0.0f});
    cameraBasePitch_ = 0.0f;
    cameraYaw_ = 0.0f;
    cameraBaseRoll_ = 0.0f;
  }

  Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());

  // テクスチャの読み込み
  TextureManager::GetInstance()->LoadTexture(crosshairPath_);
  TextureManager::GetInstance()->LoadTexture("Particles/circle.png");

  // モデル読み込み
  ModelManager::GetInstance()->LoadModel(enemyModelDirectory_, enemyModel_);
  ModelManager::GetInstance()->LoadModel(bulletModelDirectory_, bulletModel_);

  // 環境マップを敵モデルに適用
  Model *enemyModel = ModelManager::GetInstance()->FindModel(enemyModel_);
  if (enemyModel) {
    enemyModel->SetEnvironmentCoefficient(1.0f); // 反射強度を最大（1.0f）に設定
  }

  // パーティクルグループの作成
  ParticleManager::GetInstance()->CreateParticleGroup(ringParticleGroupName_,
                                                      "Particles/circle.png");

  // エミッター初期化（RingShapeEffect）
  Transform ringEmitterTransform = {
      {1.000f, 1.000f, 1.000f}, // scale
      {0.300f, 0.000f, 0.000f}, // rotate
      {-0.060f, 2.320f, 0.000f} // translate
  };
  ParticleEmitter ringEmitter(ringEmitterTransform, 32, 0.400f);
  ringEmitter.isEmit = false;
  ringEmitter.isEffectMode = true;
  ringEmitter.isLoop = false;

  ringEmitter.generateSettings.isRandomScale = false;
  ringEmitter.generateSettings.fixedScale = {1.000f, 2.000f, 4.000f};

  ringEmitter.generateSettings.isRandomRotate = true;
  ringEmitter.generateSettings.rotateMin = {0.000f, 0.000f, -3.142f};
  ringEmitter.generateSettings.rotateMax = {0.000f, 0.000f, 3.142f};

  ringEmitter.generateSettings.isRandomVelocity = false;
  ringEmitter.generateSettings.fixedVelocity = {0.000f, -2.000f, 15.000f};

  ringEmitter.generateSettings.isRandomLifeTime = false;
  ringEmitter.generateSettings.fixedLifeTime = 1.000f;

  ringEmitter.generateSettings.isRandomColor = true;
  ringEmitter.generateSettings.colorMin = {0.0f, 0.0f, 0.0f, 1.0f};
  ringEmitter.generateSettings.colorMax = {1.0f, 1.0f, 1.0f, 1.0f};

  ringEmitter.fieldSettings.isAccelerationFieldActive = false;
  ringEmitter.fieldSettings.isGravityFieldActive = false;

  ringEmitter.uvAnimationSettings.isActive = true;
  ringEmitter.uvAnimationSettings.isIndividual = true;
  ringEmitter.uvAnimationSettings.scrollSpeed = {2.000f, 0.000f};
  ringEmitter.uvAnimationSettings.rotateSpeed = 0.180f;
  ringEmitter.uvAnimationSettings.scaleSpeed = {1.500f, -1.200f};

  ringEmitter.SetShapeType(ParticleShapeType::Ring);
  if (auto *rs = ringEmitter.GetRingShape()) {
    rs->settings.innerRadius = 0.010f;
    rs->settings.startOuterRadius = 1.000f;
    rs->settings.midOuterRadius = 1.200f;
    rs->settings.endOuterRadius = 1.500f;
    rs->settings.startAngle = 0.000f;
    rs->settings.endAngle = 360.000f;
    rs->settings.division = 32;
    rs->settings.isUvSwap = false;
    rs->settings.innerColor = {1.000f, 1.000f, 1.000f, 1.000f};
    rs->settings.outerColor = {1.000f, 1.000f, 1.000f, 1.000f};
    rs->settings.fadeStartAlpha = 1.000f;
    rs->settings.fadeEndAlpha = 1.000f;
    rs->settings.fadeRange = 0.000f;
  }
  ParticleManager::GetInstance()->SetEmitter(ringParticleGroupName_,
                                             ringEmitter);

  // 追加エフェクトグループの生成・初期化
  // 1. シリンダー撃破エフェクト
  ParticleManager::GetInstance()->CreateParticleGroup("CylinderGroup",
                                                      "Particles/circle.png");
  {
    Transform cylinderTransform = {
        {1.000f, 1.000f, 1.000f}, // scale
        {0.000f, 0.000f, 0.000f}, // rotate
        {0.000f, 0.000f, 0.000f}  // translate
    };
    ParticleEmitter cylinderEmitter(cylinderTransform, 16, 0.400f);
    cylinderEmitter.isEmit = false;
    cylinderEmitter.isEffectMode = true;
    cylinderEmitter.isLoop = false;
    cylinderEmitter.generateSettings.isRandomScale = false;
    cylinderEmitter.generateSettings.fixedScale = {0.500f, 2.000f, 0.500f};
    cylinderEmitter.generateSettings.isRandomRotate = false;
    cylinderEmitter.generateSettings.fixedRotate = {0.000f, 0.000f, 0.000f};
    cylinderEmitter.generateSettings.isRandomVelocity = false;
    cylinderEmitter.generateSettings.fixedVelocity = {0.000f, 3.000f, 0.000f};
    cylinderEmitter.generateSettings.isRandomLifeTime = false;
    cylinderEmitter.generateSettings.fixedLifeTime = 0.800f;
    cylinderEmitter.generateSettings.isRandomColor = true;
    cylinderEmitter.generateSettings.colorMin = {0.0f, 0.5f, 0.5f, 1.0f};
    cylinderEmitter.generateSettings.colorMax = {0.5f, 1.0f, 1.0f, 1.0f};
    cylinderEmitter.fieldSettings.isAccelerationFieldActive = false;
    cylinderEmitter.fieldSettings.isGravityFieldActive = false;
    cylinderEmitter.uvAnimationSettings.isActive = true;
    cylinderEmitter.uvAnimationSettings.isIndividual = true;
    cylinderEmitter.uvAnimationSettings.scrollSpeed = {0.000f, -2.000f};
    cylinderEmitter.uvAnimationSettings.rotateSpeed = 0.000f;
    cylinderEmitter.uvAnimationSettings.scaleSpeed = {0.500f, 1.500f};
    cylinderEmitter.SetShapeType(ParticleShapeType::Cylinder);
    if (auto *cs = cylinderEmitter.GetCylinderShape()) {
      cs->settings.height = 1.0f;
      cs->settings.topRadius = {0.1f, 0.1f};
      cs->settings.bottomRadius = {0.1f, 0.1f};
      cs->settings.startAngle = 0.0f;
      cs->settings.endAngle = 360.0f;
      cs->settings.division = 32;
      cs->settings.verticalDivision = 1;
      cs->settings.flipV = false;
      cs->settings.isUvSwap = false;
      cs->settings.topColor = {1.0f, 1.0f, 1.0f, 1.0f};
      cs->settings.bottomColor = {1.0f, 1.0f, 1.0f, 1.0f};
      cs->settings.fadeStartAlpha = 1.000f;
      cs->settings.fadeEndAlpha = 1.000f;
      cs->settings.fadeRange = 0.000f;
      cs->settings.alphaReference = 0.0f;
    }
    ParticleManager::GetInstance()->SetEmitter("CylinderGroup",
                                               cylinderEmitter);
  }

  // 2. スパーク撃破エフェクト
  ParticleManager::GetInstance()->CreateParticleGroup("SparkGroup",
                                                      "white.png");
  {
    Transform sparkTransform = {
        {1.000f, 1.000f, 1.000f}, // scale
        {0.000f, 0.000f, 0.000f}, // rotate
        {0.000f, 0.000f, 0.000f}  // translate
    };
    ParticleEmitter sparkEmitter(sparkTransform, 30, 0.400f);
    sparkEmitter.isEmit = false;
    sparkEmitter.isEffectMode = true;
    sparkEmitter.isLoop = false;
    sparkEmitter.generateSettings.isRandomScale = true;
    sparkEmitter.generateSettings.scaleMin = {0.05f, 0.05f, 0.05f};
    sparkEmitter.generateSettings.scaleMax = {0.20f, 0.20f, 0.20f};
    sparkEmitter.generateSettings.isRandomRotate = true;
    sparkEmitter.generateSettings.rotateMin = {0.0f, 0.0f, -3.14f};
    sparkEmitter.generateSettings.rotateMax = {0.0f, 0.0f, 3.14f};
    sparkEmitter.generateSettings.isRandomVelocity = true;
    sparkEmitter.generateSettings.velocityMin = {-3.0f, 1.0f, -3.0f};
    sparkEmitter.generateSettings.velocityMax = {3.0f, 6.0f, 3.0f};
    sparkEmitter.generateSettings.isRandomLifeTime = true;
    sparkEmitter.generateSettings.lifeTimeMin = 0.3f;
    sparkEmitter.generateSettings.lifeTimeMax = 0.6f;
    sparkEmitter.generateSettings.isRandomColor = true;
    sparkEmitter.generateSettings.colorMin = {1.0f, 0.4f, 0.0f, 1.0f};
    sparkEmitter.generateSettings.colorMax = {1.0f, 1.0f, 0.0f, 1.0f};
    sparkEmitter.fieldSettings.isAccelerationFieldActive = false;
    sparkEmitter.fieldSettings.isGravityFieldActive = true;
    sparkEmitter.fieldSettings.gravity = {0.0f, -9.8f, 0.0f};
    sparkEmitter.uvAnimationSettings.isActive = false;
    sparkEmitter.SetShapeType(ParticleShapeType::Billboard);
    ParticleManager::GetInstance()->SetEmitter("SparkGroup", sparkEmitter);
  }

  // 3. リロード完了サークルエフェクト
  ParticleManager::GetInstance()->CreateParticleGroup("ReloadCompleteGroup",
                                                      "Particles/circle.png");
  {
    Transform reloadCompleteTransform = {
        {1.000f, 1.000f, 1.000f}, // scale
        {0.000f, 0.000f, 0.000f}, // rotate
        {0.000f, 0.000f, 0.000f}  // translate
    };
    ParticleEmitter reloadCompleteEmitter(reloadCompleteTransform, 8, 0.400f);
    reloadCompleteEmitter.isEmit = false;
    reloadCompleteEmitter.isEffectMode = true;
    reloadCompleteEmitter.isLoop = false;
    reloadCompleteEmitter.generateSettings.isRandomScale = false;
    reloadCompleteEmitter.generateSettings.fixedScale = {0.500f, 0.500f,
                                                         1.000f};
    reloadCompleteEmitter.generateSettings.isRandomRotate = true;
    reloadCompleteEmitter.generateSettings.rotateMin = {0.0f, 0.0f, -3.14f};
    reloadCompleteEmitter.generateSettings.rotateMax = {0.0f, 0.0f, 3.14f};
    reloadCompleteEmitter.generateSettings.isRandomVelocity = false;
    reloadCompleteEmitter.generateSettings.fixedVelocity = {0.000f, 0.000f,
                                                            0.000f};
    reloadCompleteEmitter.generateSettings.isRandomLifeTime = false;
    reloadCompleteEmitter.generateSettings.fixedLifeTime = 0.600f;
    reloadCompleteEmitter.generateSettings.isRandomColor = true;
    reloadCompleteEmitter.generateSettings.colorMin = {0.0f, 0.8f, 0.2f, 1.0f};
    reloadCompleteEmitter.generateSettings.colorMax = {0.5f, 1.0f, 0.5f, 1.0f};
    reloadCompleteEmitter.fieldSettings.isAccelerationFieldActive = false;
    reloadCompleteEmitter.fieldSettings.isGravityFieldActive = false;
    reloadCompleteEmitter.uvAnimationSettings.isActive = true;
    reloadCompleteEmitter.uvAnimationSettings.isIndividual = true;
    reloadCompleteEmitter.uvAnimationSettings.scrollSpeed = {1.500f, 0.000f};
    reloadCompleteEmitter.uvAnimationSettings.rotateSpeed = 0.100f;
    reloadCompleteEmitter.uvAnimationSettings.scaleSpeed = {2.500f, -2.000f};
    reloadCompleteEmitter.SetShapeType(ParticleShapeType::Ring);
    if (auto *rs = reloadCompleteEmitter.GetRingShape()) {
      rs->settings.innerRadius = 0.010f;
      rs->settings.startOuterRadius = 0.500f;
      rs->settings.midOuterRadius = 1.500f;
      rs->settings.endOuterRadius = 3.000f;
      rs->settings.startAngle = 0.000f;
      rs->settings.endAngle = 360.000f;
      rs->settings.division = 32;
      rs->settings.isUvSwap = false;
      rs->settings.innerColor = {1.000f, 1.000f, 1.000f, 1.000f};
      rs->settings.outerColor = {1.000f, 1.000f, 1.000f, 1.000f};
      rs->settings.fadeStartAlpha = 1.000f;
      rs->settings.fadeEndAlpha = 1.000f;
      rs->settings.fadeRange = 0.000f;
    }
    ParticleManager::GetInstance()->SetEmitter("ReloadCompleteGroup",
                                               reloadCompleteEmitter);
  }

  // 4. アモUI消費スパークエフェクト
  ParticleManager::GetInstance()->CreateParticleGroup("AmmoSparkGroup",
                                                      "white.png");
  {
    Transform ammoSparkTransform = {
        {1.000f, 1.000f, 1.000f}, // scale
        {0.000f, 0.000f, 0.000f}, // rotate
        {0.000f, 0.000f, 0.000f}  // translate
    };
    ParticleEmitter ammoSparkEmitter(ammoSparkTransform, 10, 0.400f);
    ammoSparkEmitter.isEmit = false;
    ammoSparkEmitter.isEffectMode = true;
    ammoSparkEmitter.isLoop = false;
    ammoSparkEmitter.generateSettings.isRandomScale = true;
    ammoSparkEmitter.generateSettings.scaleMin = {0.01f, 0.01f, 0.01f};
    ammoSparkEmitter.generateSettings.scaleMax = {0.02f, 0.02f, 0.02f};
    ammoSparkEmitter.generateSettings.isRandomRotate = true;
    ammoSparkEmitter.generateSettings.rotateMin = {0.0f, 0.0f, -3.14f};
    ammoSparkEmitter.generateSettings.rotateMax = {0.0f, 0.0f, 3.14f};
    ammoSparkEmitter.generateSettings.isRandomVelocity = true;
    ammoSparkEmitter.generateSettings.velocityMin = {-0.5f, 0.4f, 0.2f};
    ammoSparkEmitter.generateSettings.velocityMax = {0.2f, 1.0f, 1.0f};
    ammoSparkEmitter.generateSettings.isRandomLifeTime = true;
    ammoSparkEmitter.generateSettings.lifeTimeMin = 0.15f;
    ammoSparkEmitter.generateSettings.lifeTimeMax = 0.35f;
    ammoSparkEmitter.generateSettings.isRandomColor = true;
    ammoSparkEmitter.generateSettings.colorMin = {1.0f, 0.6f, 0.1f, 1.0f};
    ammoSparkEmitter.generateSettings.colorMax = {1.0f, 0.8f, 0.2f, 1.0f};
    ammoSparkEmitter.fieldSettings.isAccelerationFieldActive = false;
    ammoSparkEmitter.fieldSettings.isGravityFieldActive = true;
    ammoSparkEmitter.fieldSettings.gravity = {0.0f, -4.0f, 0.0f};
    ammoSparkEmitter.uvAnimationSettings.isActive = false;
    ammoSparkEmitter.SetShapeType(ParticleShapeType::Billboard);
    ParticleManager::GetInstance()->SetEmitter("AmmoSparkGroup",
                                               ammoSparkEmitter);
  }

  // 5. 敵弾トレイルエフェクト (半円シリンダーによるライフリング効果)
  auto setupBulletTrailGroup = [](const std::string& groupName, float radius, float height, const Vector4& topCol, const Vector4& btmCol) {
    ParticleManager::GetInstance()->CreateParticleGroup(groupName, "white.png");
    Transform t = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    ParticleEmitter emitter(t, 1, 0.1f);
    emitter.isEmit = false;
    emitter.isEffectMode = false;
    emitter.generateSettings.isRandomScale = false;
    emitter.generateSettings.fixedScale = { 1.0f, 1.0f, 1.0f };
    emitter.generateSettings.isRandomRotate = false;
    emitter.generateSettings.fixedRotate = { 0.0f, 0.0f, 0.0f };
    emitter.generateSettings.isRandomVelocity = false;
    emitter.generateSettings.fixedVelocity = { 0.0f, 0.0f, 0.0f };
    emitter.generateSettings.isRandomLifeTime = false;
    emitter.generateSettings.fixedLifeTime = 0.3f;
    emitter.generateSettings.isRandomColor = false;
    emitter.generateSettings.fixedColor = topCol;
    emitter.fieldSettings.isAccelerationFieldActive = false;
    emitter.fieldSettings.isGravityFieldActive = false;
    emitter.uvAnimationSettings.isActive = false;
    emitter.SetShapeType(ParticleShapeType::Cylinder);
    if (auto* cs = emitter.GetCylinderShape()) {
      cs->settings.height = height;
      cs->settings.topRadius = { radius, radius };
      cs->settings.bottomRadius = { radius, radius };
      cs->settings.startAngle = 0.0f;
      cs->settings.endAngle = 180.0f; // 半円状
      cs->settings.division = 24;
      cs->settings.verticalDivision = 1;
      cs->settings.topColor = topCol;
      cs->settings.bottomColor = btmCol;
      cs->settings.fadeStartAlpha = 1.0f;
      cs->settings.fadeEndAlpha = 1.0f;
      cs->settings.fadeRange = 0.15f; // 両端フェード
      cs->settings.alphaReference = 0.0f;
    }
    ParticleManager::GetInstance()->SetEmitter(groupName, emitter);
  };

  setupBulletTrailGroup("BulletTrail_Normal", 0.45f, 0.12f, { 0.9f, 0.95f, 1.0f, 0.85f }, { 0.8f, 0.9f, 1.0f, 0.7f });
  setupBulletTrailGroup("BulletTrail_Blast", 0.65f, 0.16f, { 1.0f, 0.75f, 0.1f, 0.9f }, { 1.0f, 0.4f, 0.0f, 0.8f });
  setupBulletTrailGroup("BulletTrail_Jamming", 0.45f, 0.12f, { 0.1f, 0.65f, 1.0f, 0.9f }, { 0.0f, 0.3f, 0.9f, 0.75f });

  // 初期化直後の余分なパーティクル描画を防ぐため全パーティクルをクリア
  ParticleManager::GetInstance()->ClearAllParticles();

  // スカイボックスの初期化
  skybox_ = std::make_unique<Skybox>();
  skybox_->Initialize("skybox.dds");
  skybox_->SetCamera(camera_.get());

  Object3dCommon::GetInstance()->SetEnvironmentMap(
      TextureManager::GetInstance()->GetSrvIndex("skybox.dds"));

  // フロアモデルをロード
  ModelManager::GetInstance()->LoadModel(floorModelDirectory_, floorModel_);

  // フロアオブジェクトの初期化
  floorObject_ = std::make_unique<Object3d>();
  floorObject_->Initialize();
  floorObject_->SetModel(floorModel_);
  floorObject_->SetTranslate(floorSettings_.position);
  floorObject_->SetRotation(floorSettings_.rotation);
  floorObject_->SetScale(floorSettings_.scale);
  floorObject_->SetCamera(camera_.get());

  // レベルデータから敵キャラを生成・配置
  enemies_.clear();
  if (levelData_) {
    for (const auto &enemyData : levelData_->enemies) {
      EnemyInfo enemy;
      enemy.object = std::make_unique<Object3d>();
      enemy.object->Initialize();

      // 個別のモデルインスタンスを生成・ロード
      enemy.model = std::make_unique<Model>();
      enemy.model->Initialize(enemyModelDirectory_, enemyModel_);
      // 初期状態として完全に消去された状態（Threshold = 1.0f）を設定
      enemy.model->SetDissolveMaskTexture("masks/noise0.png");
      enemy.model->SetDissolveParams(1, 1.0f, 0.05f, Vector3(1.0f, 0.4f, 0.3f));

      enemy.object->SetModel(enemy.model.get());
      enemy.object->SetTranslate(enemyData.translation);
      enemy.object->SetRotation(enemyData.rotation);
      enemy.object->SetCamera(camera_.get());
      enemy.basePosition = enemyData.translation;
      enemy.baseRotation = enemyData.rotation;
      enemy.distance = enemyData.distance;
      enemy.isActive = false;
      enemy.isDead = false;
      enemy.shootTimer = 0.0f;
      enemy.spawnTimer = 0.0f;
      enemies_.push_back(std::move(enemy));
    }
    
    // 3体目の敵（インデックス2）を工兵（Engineer）タイプに設定
    if (enemies_.size() >= 3) {
      enemies_[2].type = EnemyType::Engineer;
      // 工兵のモデルカラーを爆発物イメージの黄色系に変更
      if (enemies_[2].model) {
        enemies_[2].model->SetColor({1.0f, 0.8f, 0.2f, 1.0f});
      }
    }
  }

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

  // --- スプライト生成 ---
  crosshair_ = std::make_unique<Sprite>();
  crosshair_->Initialize(crosshairPath_);
  crosshair_->SetAnchorPoint({0.5f, 0.5f});
  crosshair_->SetScale({64.0f, 64.0f});

  // マガジンとカバーの初期化
  ammo_ = kMaxAmmo;
  isCovering_ = false;
  reloadTimer_ = 0.0f;

  // 初期フェーズの設定
  phase_ = Phase::Playing;
  hitCount_ = 0;

  // -----------------------------------------------------------------
  // UIスプライトの初期化
  // -----------------------------------------------------------------
  TextureManager::GetInstance()->LoadTexture("white.png");

  // 1. ライフUI
  lifeBg_ = std::make_unique<Sprite>();
  lifeBg_->Initialize("white.png");
  lifeBg_->SetTranslate({1140.0f, 30.0f});
  lifeBg_->SetScale({110.0f, 50.0f});
  lifeBg_->SetColor({0.2f, 0.2f, 0.2f, 0.8f}); // 半透明グレー

  lifeUnits_.clear();
  lifeCurrentX_.clear();
  lifeTargetX_.clear();
  for (int i = 0; i < kMaxHits; ++i) {
    auto unit = std::make_unique<Sprite>();
    unit->Initialize("white.png");
    unit->SetScale({16.0f, 36.0f});
    unit->SetColor({1.0f, 0.0f, 0.0f, 1.0f}); // 赤色

    float initX = 1214.0f - (kMaxHits - 1 - i) * 24.0f;
    lifeUnits_.push_back(std::move(unit));
    lifeCurrentX_.push_back(initX);
    lifeTargetX_.push_back(initX);
  }

  // 2. 弾薬UI
  ammoBg_ = std::make_unique<Sprite>();
  ammoBg_->Initialize("white.png");
  ammoBg_->SetTranslate({30.0f, 640.0f});
  ammoBg_->SetScale({200.0f, 40.0f});
  ammoBg_->SetColor({0.2f, 0.2f, 0.2f, 0.8f}); // 半透明グレー

  ammoUnits_.clear();
  ammoCurrentX_.clear();
  ammoTargetX_.clear();
  for (int i = 0; i < kMaxAmmo; ++i) {
    auto unit = std::make_unique<Sprite>();
    unit->Initialize("white.png");
    unit->SetScale({12.0f, 24.0f});
    unit->SetColor(
        {183.0f / 255.0f, 132.0f / 255.0f, 48.0f / 255.0f, 1.0f}); // 茶色系

    float initX = 202.0f - i * 20.0f;
    ammoUnits_.push_back(std::move(unit));
    ammoCurrentX_.push_back(initX);
    ammoTargetX_.push_back(initX);
  }

  // 3. プログレスバーUI
  progressBg_ = std::make_unique<Sprite>();
  progressBg_->Initialize("white.png");
  progressBg_->SetTranslate({480.0f, 20.0f});
  progressBg_->SetScale({300.0f, 20.0f});
  progressBg_->SetColor({0.1f, 0.2f, 0.8f, 0.6f}); // 半透明青色

  progressBar_ = std::make_unique<Sprite>();
  progressBar_->Initialize("white.png");
  progressBar_->SetTranslate({480.0f, 20.0f});
  progressBar_->SetScale({0.0f, 20.0f});            // 最初は幅0
  progressBar_->SetColor({1.0f, 0.9f, 0.0f, 1.0f}); // 黄色

  // 4. カバー演出UI
  coverOverlay_ = std::make_unique<Sprite>();
  coverOverlay_->Initialize("white.png");
  coverOverlay_->SetTranslate({0.0f, 470.0f});
  coverOverlay_->SetScale({1280.0f, 250.0f});
  coverOverlay_->SetColor({0.0f, 0.0f, 0.0f, 0.6f}); // 半透明黒

  // 5. スコアUIの初期化
  for (int i = 0; i < 10; ++i) {
    TextureManager::GetInstance()->LoadTexture("numbers/" + std::to_string(i) + ".png");
  }

  scoreBg_ = std::make_unique<Sprite>();
  scoreBg_->Initialize("white.png");
  scoreBg_->SetTranslate({30.0f, 30.0f});
  scoreBg_->SetScale({160.0f, 45.0f});
  scoreBg_->SetColor({0.2f, 0.2f, 0.2f, 0.8f}); // 半透明グレー

  scoreUnits_.clear();
  for (int i = 0; i < 4; ++i) {
    auto unit = std::make_unique<Sprite>();
    unit->Initialize("numbers/0.png");
    unit->SetScale({20.0f, 32.0f});
    unit->SetTranslate({40.0f + i * 30.0f, 36.5f});
    scoreUnits_.push_back(std::move(unit));
  }

  // 6. タイマーUIの初期化
  timerBg_ = std::make_unique<Sprite>();
  timerBg_->Initialize("white.png");
  timerBg_->SetTranslate({1090.0f, 640.0f});
  timerBg_->SetScale({160.0f, 45.0f});
  timerBg_->SetColor({0.2f, 0.2f, 0.2f, 0.8f}); // 半透明グレー

  timerUnits_.clear();
  for (int i = 0; i < 4; ++i) {
    auto unit = std::make_unique<Sprite>();
    unit->Initialize("numbers/0.png");
    unit->SetScale({20.0f, 32.0f});
    float x = 1100.0f + i * 30.0f;
    if (i >= 2) {
      x += 7.0f; // ドット用の隙間
    }
    unit->SetTranslate({x, 646.5f});
    timerUnits_.push_back(std::move(unit));
  }

  timerDot_ = std::make_unique<Sprite>();
  timerDot_->Initialize("white.png");
  timerDot_->SetTranslate({1158.0f, 672.0f});
  timerDot_->SetScale({4.0f, 4.0f});
  timerDot_->SetColor({1.0f, 1.0f, 1.0f, 1.0f}); // 白色

  score_ = 0;
  gameTimer_ = 60.0f;

  // スウォーム（空中群体制御）システムの初期化
  swarmManager_ = std::make_unique<SwarmManager>();
  swarmManager_->Initialize(128, enemyModelDirectory_, enemyModel_);
}

void ShootingScene::Update() {
  // 入力の更新は常に行う
  Input::GetInstance()->Update();

  // ポーズ（TAB）のトグル検出
  if (Input::GetInstance()->IsKeyTriggered(DIK_TAB)) {
    if (!isPaused_) {
      isPaused_ = true;
      prevPostEffectMode_ = PostProcessManager::GetInstance()->GetCurrentMode();
      MyGame::SetPostEffectMode(PostProcessManager::kModeGaussianBlur);
      PostProcessManager::SetGaussianBlurParams(7, 2.0f);
    } else {
      isPaused_ = false;
      MyGame::SetPostEffectMode(prevPostEffectMode_);
    }
  }

#ifdef USE_IMGUI
  // UI処理 (ImGuiの定義)
  UpdateImGui();
#endif // USE_IMGUI

  if (isPaused_ || isDebugPaused_) {
    camera_->Update();
    return;
  }

  // 現在の cameraProgress_ から currentSectionIndex_ を更新
  for (int i = static_cast<int>(sectionProgresses_.size()) - 1; i >= 0; --i) {
    if (cameraProgress_ >= sectionProgresses_[i]) {
      currentSectionIndex_ = i;
      break;
    }
  }

  // 無敵タイマーの更新
  if (sectionJumpInvincibleTimer_ > 0.0f) {
    sectionJumpInvincibleTimer_ -= kDeltaTime;
    if (sectionJumpInvincibleTimer_ < 0.0f) {
      sectionJumpInvincibleTimer_ = 0.0f;
    }
  }

  // 制限時間の更新
  if (phase_ == Phase::Playing) {
    gameTimer_ -= kDeltaTime;
    if (gameTimer_ <= 0.0f) {
      gameTimer_ = 0.0f;

      // タイムアップによるゲームオーバー遷移
      phase_ = Phase::GameOverVignette;
      phaseTimer_ = 0.0f;
      vignetteScale_ = 16.0f;
      vignetteExponent_ = 0.8f;
      damageEffectStrength_ = 0.0f;

      MyGame::SetPostEffectMode(PostProcessManager::kModeVignette);
      MyGame::SetPostEffectStrength(1.0f);
      PostProcessManager::SetVignetteParams(vignetteScale_,
                                            vignetteExponent_);
      projectiles_.clear();

      return; // ここでUpdateを抜けてゲームを一時停止させる
    }
  }

  const float kHalfPI = std::numbers::pi_v<float> / 2.0f;

  // =====================================================
  // フェーズ: ゲームオーバービネット（更新停止・ビネット強化）
  // =====================================================
  if (phase_ == Phase::GameOverVignette) {
      phaseTimer_ += kDeltaTime;
      float t = phaseTimer_ / kVignetteInDuration; // 0.0 → 1.0
      if (t > 1.0f)
          t = 1.0f;

      // ビネットを徐々に強化: scale 16→0.5, exponent 0.8→6.0
      vignetteScale_ = 16.0f + (0.5f - 16.0f) * t;
      vignetteExponent_ = 0.8f + (6.0f - 0.8f) * t;
      PostProcessManager::SetMode(PostProcessManager::kModeVignette);
      PostProcessManager::SetVignetteParams(vignetteScale_, vignetteExponent_);

      if (phaseTimer_ >= kVignetteInDuration) {
#ifndef USE_IMGUI
          // USE_IMGUIでない場合はGameOverSceneへ遷移
          PostProcessManager::SetMode(PostProcessManager::kModeCopy);
          SceneManager::GetInstance()->ChangeScene("GAMEOVER");
          return;
#else
          // USE_IMGUIの場合は暗転維持フェーズを経てその場でリスタート
          phase_ = Phase::GameOverWait;
          phaseTimer_ = 0.0f;
#endif
          return;
      }
  }
  // =====================================================
  // フェーズ: スウォーム戦闘（約100機がプレイヤー周辺20m以内を旋回・突撃）
  // =====================================================
  if (phase_ == Phase::SwarmBattle) {
    swarmBattleTimer_ += kDeltaTime;

    // 制限時間経過（例: 25秒）で最終クリア暗転へ遷移
    const float kSwarmBattleDuration = 25.0f;
    if (swarmBattleTimer_ >= kSwarmBattleDuration) {
      phase_ = Phase::ClearVignette;
      phaseTimer_ = 0.0f;
      vignetteScale_ = 16.0f;
      vignetteExponent_ = 0.8f;
      damageEffectStrength_ = 0.0f;

      MyGame::SetPostEffectMode(PostProcessManager::kModeVignette);
      MyGame::SetPostEffectStrength(1.0f);
      PostProcessManager::SetVignetteParams(vignetteScale_, vignetteExponent_);
      projectiles_.clear();
      return;
    }
  }

  // =====================================================
  // フェーズ: クリア暗転（ビネットで覆っていきClearSceneへ遷移）
  // =====================================================
  if (phase_ == Phase::ClearVignette) {
    phaseTimer_ += kDeltaTime;
    float t = phaseTimer_ / kVignetteInDuration; // 0.0 → 1.0
    if (t > 1.0f)
      t = 1.0f;

    // ビネットを徐々に強化: scale 16→0.5, exponent 0.8→6.0
    vignetteScale_ = 16.0f + (0.5f - 16.0f) * t;
    vignetteExponent_ = 0.8f + (6.0f - 0.8f) * t;
    PostProcessManager::SetMode(PostProcessManager::kModeVignette);
    PostProcessManager::SetVignetteParams(vignetteScale_, vignetteExponent_);

    if (phaseTimer_ >= kVignetteInDuration) {
      PostProcessManager::SetMode(PostProcessManager::kModeCopy);
      ClearScene::SetResultData(score_, gameTimer_);
      SceneManager::GetInstance()->ChangeScene("CLEAR");
      return;
    }

    return;
  }

  // =====================================================
  // フェーズ: ゲームオーバー暗転維持（3秒間停止）
  // =====================================================
  if (phase_ == Phase::GameOverWait) {
    phaseTimer_ += kDeltaTime;

    if (phaseTimer_ >= kGameOverWaitDuration) {
      // --- シーンを初期化（論理リセット） ---
      hitCount_ = 0;
      orbitAngle_ = 0.0f;
      targetTimer_ = 0.0f;
      cameraYaw_ = 0.0f;
      isHit_ = false;
      damageEffectStrength_ = 0.0f;
      projectileSpawnTimer_ = 0.0f;
      isCovering_ = false;
      coverYOffset_ = 0.0f;
      isPaused_ = false;
      projectiles_.clear();
      score_ = 0;
      gameTimer_ = 60.0f;
      radialBlurStrength_ = 0.0f;
      radialBlurTimer_ = 0.0f;
      randomNoiseStrength_ = 0.0f;
      randomNoiseTimer_ = 0.0f;

      // 敵の論理リセット（モデルのリロードは行わない）
      for (auto &enemy : enemies_) {
        enemy.isDead = false;
        enemy.isActive = false;
        enemy.shootTimer = 0.0f;
        enemy.shootCount = 0;
        enemy.hitShotCount = 0;
        enemy.spawnTimer = 0.0f;
        enemy.object->SetTranslate(enemy.basePosition);
        enemy.object->SetRotation(enemy.baseRotation);
        // 敵のタイプに応じたモデルカラーの再適用
        if (enemy.model) {
          if (enemy.type == EnemyType::Engineer) {
            enemy.model->SetColor({1.0f, 0.8f, 0.2f, 1.0f});
          } else {
            enemy.model->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
          }
        }
        // 初期状態として完全に消去された状態（Threshold = 1.0f）を設定
        enemy.model->SetDissolveParams(1, 1.0f, 0.05f,
                                       Vector3(1.0f, 0.4f, 0.3f));
        enemy.object->Update(mainViewIndex_, camera_.get());
      }

      ammo_ = kMaxAmmo;
      isCovering_ = false;
      reloadTimer_ = 0.0f;

      // UIのイージング座標を強制初期化（ちらつき防止）
      int currentLife = kMaxHits - hitCount_;
      for (int i = 0; i < kMaxHits; ++i) {
        float initX = 1214.0f - (kMaxHits - 1 - i) * 24.0f;
        lifeCurrentX_[i] = initX;
        lifeTargetX_[i] = initX;
        if (i < currentLife) {
          lifeUnits_[i]->SetTranslate({initX, 37.0f});
          lifeUnits_[i]->Update();
        }
      }

      for (int i = 0; i < kMaxAmmo; ++i) {
        float initX = 202.0f - i * 20.0f;
        ammoCurrentX_[i] = initX;
        ammoTargetX_[i] = initX;
        ammoUnits_[i]->SetTranslate({initX, 648.0f});
        ammoUnits_[i]->SetColor(
            {183.0f / 255.0f, 132.0f / 255.0f, 48.0f / 255.0f, 1.0f});
        ammoUnits_[i]->Update();
      }

      // カメラを初期位置に戻す
      cameraProgress_ = 0.0f;
      cameraShakeTimer_ = 0.0f;
      isMovementPaused_ = false;
      Vector3 startCamPos = CalculateRailPosition(cameraProgress_);
      camera_->SetTranslate(startCamPos);
      if (levelData_ && !levelData_->players.empty()) {
        const auto &spawn = levelData_->players[0];
        camera_->SetRotate(spawn.rotation);
        cameraBasePitch_ = spawn.rotation.x;
        cameraYaw_ = spawn.rotation.y;
        cameraBaseRoll_ = spawn.rotation.z;
      } else {
        camera_->SetRotate({0.0f, 0.0f, 0.0f});
        cameraBasePitch_ = 0.0f;
        cameraYaw_ = 0.0f;
        cameraBaseRoll_ = 0.0f;
      }

      // 残留パーティクルを完全にクリア
      ParticleManager::GetInstance()->ClearAllParticles();

      // スムージングを最大限（31）適用してリスタート
      smoothingKernel_ = 31.0f;
      PostProcessManager::SetMode(PostProcessManager::kModeSmoothing);
      PostProcessManager::SetSmoothingParams(
          static_cast<int>(smoothingKernel_));

      phase_ = Phase::RestartSmoothing;
      phaseTimer_ = 0.0f;
    }

    return;
  }

  // =====================================================
  // フェーズ: リスタートスムージングフェードアウト
  // =====================================================
  if (phase_ == Phase::RestartSmoothing) {
    phaseTimer_ += kDeltaTime;
    float t = phaseTimer_ / kSmoothingOutDuration; // 0.0 → 1.0
    if (t > 1.0f)
      t = 1.0f;

    // カーネルサイズを 31 → 1 に線形補間
    smoothingKernel_ = 31.0f + (1.0f - 31.0f) * t;
    int kernel = static_cast<int>(smoothingKernel_);
    if (kernel % 2 == 0)
      kernel++; // ぼかしフィルタの仕様上、奇数に補正
    if (kernel < 1)
      kernel = 1;

    PostProcessManager::SetMode(PostProcessManager::kModeSmoothing);
    PostProcessManager::SetSmoothingParams(kernel);

    if (phaseTimer_ >= kSmoothingOutDuration) {
      // 通常画面になったら完全に通常コピーモードへ戻す
      PostProcessManager::SetMode(PostProcessManager::kModeCopy);
      phase_ = Phase::Playing;
    }
    // スムージング中もゲームは再開しているため、このまま下の通常ロジックを走らせる
  }

  // =====================================================
  // 通常プレイ時のゲームロジック (Playing / RestartSmoothing共通)
  // =====================================================

  // 隠れるアクションの更新
  isCovering_ = Input::GetInstance()->IsKeyDown(DIK_SPACE);
  if (isCovering_) {
    if (ammo_ < kMaxAmmo) {
      reloadTimer_ += kDeltaTime;
      if (reloadTimer_ >= kReloadDuration) {
        ammo_ = kMaxAmmo;
        reloadTimer_ = 0.0f;

        // リロード完了エフェクトをカメラの目の前に発生させる
        Vector3 camPos = camera_->GetTranslate();
        Vector3 camRot = camera_->GetRotate();
        float sinY = std::sin(camRot.y);
        float cosY = std::cos(camRot.y);
        Vector3 forward = {sinY, 0.0f, cosY};
        Vector3 spawnPos = Add(camPos, Multiply(1.5f, forward));

        if (auto *emitter = ParticleManager::GetInstance()->GetEmitter(
                "ReloadCompleteGroup")) {
          emitter->isPlaying = true;
        }
        ParticleManager::GetInstance()->Emit("ReloadCompleteGroup", spawnPos,
                                             8);
      }
    }
  } else {
    reloadTimer_ = 0.0f;
  }

  // --- レール移動の更新 ---
  UpdateRailMovement();

  // カバーYオフセットのイージング更新（スムーズなしゃがみ・立ち上がり）
  float targetOffset = isCovering_ ? -1.0f : 0.0f;
  coverYOffset_ += (targetOffset - coverYOffset_) * 0.15f;

  // カメラシェイクオフセットの計算（被弾時の画面揺れ）
  Vector3 shakeOffset = {0.0f, 0.0f, 0.0f};
  Vector3 shakeRotOffset = {0.0f, 0.0f, 0.0f};
  if (cameraShakeTimer_ > 0.0f) {
    cameraShakeTimer_ -= kDeltaTime;
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

  Vector3 camPos = CalculateRailPosition(cameraProgress_);
  camPos.y += coverYOffset_;
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

  // 基準回転（basePitch, cameraYaw, baseRoll）にシェイクオフセットを加算（累積せず完全復元）
  camera_->SetRotate({
    cameraBasePitch_ + shakeRotOffset.x,
    cameraYaw_ + shakeRotOffset.y,
    cameraBaseRoll_ + shakeRotOffset.z
  });

  // 各エネミーの更新と射撃処理
  for (auto &enemy : enemies_) {
    if (!enemy.isActive || enemy.isDead)
      continue;

    // 出現（Dissolve In）のアニメーション更新
    if (enemy.spawnTimer < 1.0f) {
      enemy.spawnTimer += kDeltaTime;
      if (enemy.spawnTimer > 1.0f) {
        enemy.spawnTimer = 1.0f;
      }
      float threshold = 1.0f - enemy.spawnTimer; // 1.0 -> 0.0

      // 出現完了ならDissolveを無効化
      int enableDissolve = (threshold > 0.0f) ? 1 : 0;
      enemy.model->SetDissolveParams(enableDissolve, threshold, 0.05f,
                                     Vector3(1.0f, 0.4f, 0.3f));
    }

    // 出現完了するまでは射撃を行わない
    if (enemy.spawnTimer >= 1.0f) {
      // 射撃処理
      enemy.shootTimer += 1.0f;
      float spawnInterval = (enemy.type == EnemyType::Engineer) ? kEngineerSpawnInterval : kProjectileSpawnInterval;
      if (enemy.shootTimer >= spawnInterval) {
        enemy.shootTimer = 0.0f;
        Vector3 startPos = enemy.object->GetTranslate();
        Vector3 playerPos = CalculateRailPosition(cameraProgress_);
        
        enemy.shootCount++;
        
        // 必中弾か演出弾かの判定 (タイプごとの確率判定)
        float currentHitShotRate = (enemy.type == EnemyType::Engineer) ? 0.75f : hitShotRate_;
        float randVal = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        bool isHitShot = (randVal < currentHitShotRate);
        
        Vector3 targetPos = playerPos;
        EnemyProjectile::Type pType = EnemyProjectile::Type::Normal;
        
        if (isHitShot) {
          // --- 必中弾 (プレイヤーを正確に狙う) ---
          enemy.hitShotCount++;
          if (enemy.type == EnemyType::Engineer) {
            pType = EnemyProjectile::Type::Blast;   // 工兵は必ず爆発弾 (黄)
          } else {
            // 通常タイプは爆発弾を撃たず、通常弾(白)とジャミング弾(青)を交互に撃ち分ける
            if (enemy.hitShotCount % 2 == 0) {
              pType = EnemyProjectile::Type::Jamming; // ジャミング弾 (青)
            } else {
              pType = EnemyProjectile::Type::Normal;  // 通常必中弾 (白)
            }
          }
        } else {
          // --- 演出弾 (プレイヤーの周囲を掠める当たらない弾) ---
          // プレイヤーの衝突判定半径(1.0f)より確実に外側の安全距離(1.8f〜missShotSpread_)にオフセット
          float minOffset = 1.8f;
          float maxOffset = (missShotSpread_ > minOffset) ? missShotSpread_ : minOffset + 1.0f;
          float angle = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
          float offsetDist = minOffset + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * (maxOffset - minOffset);
          
          targetPos.x += std::cos(angle) * offsetDist;
          targetPos.y += std::sin(angle) * offsetDist * 0.7f; // 上下のオフセット
          
          if (enemy.type == EnemyType::Engineer) {
            pType = EnemyProjectile::Type::Blast;   // 工兵は演出弾も爆発弾 (黄)
          } else {
            pType = EnemyProjectile::Type::Normal;  // 通常タイプは通常演出弾 (白)
          }
        }
        
        Vector3 dir = Normalize(Subtract(targetPos, startPos));
        float bulletSpeed = (enemy.type == EnemyType::Engineer) ? engineerProjectileSpeed_ : speed_;
        Vector3 velocity = Multiply(bulletSpeed, dir);
        
        auto newProjectile = std::make_unique<EnemyProjectile>();
        newProjectile->Initialize(startPos, velocity, pType);
        projectiles_.push_back(std::move(newProjectile));
      }
    }

    // オブジェクトの更新
    enemy.object->Update(mainViewIndex_, camera_.get());
  }

  // スカイボックスの更新
  skybox_->Update(mainViewIndex_, camera_.get());

  // フロアオブジェクトの更新
  floorObject_->Update(mainViewIndex_, camera_.get());

  // プロジェクタイルの移動・行列更新
  for (auto &p : projectiles_) {
    p->Update();
    p->Update(mainViewIndex_, camera_.get());
  }

  // カメラとの当たり判定（プレイヤー被弾処理）
  bool isInvincible = IsInvincible();

  if (isCovering_) {
    // 遮蔽中の場合、被弾予定位置（遮蔽前の本来のプレイヤー位置）を通過した弾を消去する
    Vector3 targetPos = CalculateRailPosition(cameraProgress_);
    for (auto &p : projectiles_) {
      if (p->IsDead())
        continue;
      Vector3 toProjectile = Subtract(p->GetPosition(), targetPos);
      if (Dot(toProjectile, p->GetVelocity()) >= 0.0f) {
        if (p->IsExplosive()) {
          radialBlurStrength_ = 0.035f; // カバー時爆発
          radialBlurTimer_ = kRadialBlurDuration;
          PostProcessManager::SetRadialBlurParams(0.5f, 0.5f, radialBlurStrength_, 12);
          cameraShakeIntensity_ = 0.20f;
          cameraShakeDuration_ = 0.30f;
          cameraShakeTimer_ = 0.30f;
        }
        p->Kill();
      }
    }

    // しゃがみ動作中でまだ完全に隠れきっていない（無敵になっていない）場合は、
    // 頭上を通る前にカメラ座標（しゃがみかけの高さ）への当たり判定を行う
    if (!isInvincible) {
      for (auto &p : projectiles_) {
        if (p->IsDead())
          continue;
        float dist =
            Length(Subtract(p->GetPosition(), camera_->GetTranslate()));
        if (dist < 1.0f) {
          if (p->IsExplosive() && hitCount_ + 1 < kMaxHits) {
            radialBlurStrength_ = 0.05f; // 被弾時
            radialBlurTimer_ = kRadialBlurDuration;
            PostProcessManager::SetRadialBlurParams(0.5f, 0.5f, radialBlurStrength_, 12);
            cameraShakeIntensity_ = 0.45f;
            cameraShakeDuration_ = 0.45f;
            cameraShakeTimer_ = 0.45f;
          } else {
            cameraShakeIntensity_ = 0.20f;
            cameraShakeDuration_ = 0.25f;
            cameraShakeTimer_ = 0.25f;
          }
          // ジャミング弾被弾時のノイズトリガー（生存時のみ、通常弾ヒット時はノイズ無し）
          if (p->IsJamming() && hitCount_ + 1 < kMaxHits) {
            randomNoiseStrength_ = 0.7f;
            randomNoiseTimer_ = 5.0f; // 5秒
            PostProcessManager::SetRandomParams(randomNoiseStrength_);
          }
          p->Kill(); // 当たった弾は必ず消す
          hitCount_++;
          damageEffectStrength_ = 1.0f;
          MyGame::SetPostEffectMode(PostProcessManager::kModeGrayscale);

          // 3回ヒットしたらゲームオーバー演出開始
          if (hitCount_ >= kMaxHits) {
            phase_ = Phase::GameOverVignette;
            phaseTimer_ = 0.0f;
            vignetteScale_ = 16.0f;
            vignetteExponent_ = 0.8f;
            damageEffectStrength_ = 0.0f;

            MyGame::SetPostEffectMode(PostProcessManager::kModeVignette);
            MyGame::SetPostEffectStrength(1.0f);
            PostProcessManager::SetVignetteParams(vignetteScale_,
                                                  vignetteExponent_);
            projectiles_.clear();

            return; // ここでUpdateを抜けてゲームを一時停止させる
          }
        }
      }
    }
  } else {
    // 遮蔽していない通常時
    for (auto &p : projectiles_) {
      if (p->IsDead())
        continue;
      float dist = Length(Subtract(p->GetPosition(), camera_->GetTranslate()));
      if (dist < 1.0f) {
        if (p->IsExplosive() && !isInvincible && hitCount_ + 1 < kMaxHits) {
          radialBlurStrength_ = 0.05f; // 被弾時
          radialBlurTimer_ = kRadialBlurDuration;
          PostProcessManager::SetRadialBlurParams(0.5f, 0.5f, radialBlurStrength_, 12);
          cameraShakeIntensity_ = 0.45f;
          cameraShakeDuration_ = 0.45f;
          cameraShakeTimer_ = 0.45f;
        } else if (!isInvincible) {
          cameraShakeIntensity_ = 0.20f;
          cameraShakeDuration_ = 0.25f;
          cameraShakeTimer_ = 0.25f;
        }
        // ジャミング弾被弾時のノイズトリガー（生存時のみ、通常弾ヒット時はノイズ無し）
        if (p->IsJamming() && !isInvincible && hitCount_ + 1 < kMaxHits) {
          randomNoiseStrength_ = 0.7f;
          randomNoiseTimer_ = 5.0f; // 5秒
          PostProcessManager::SetRandomParams(randomNoiseStrength_);
        }
        p->Kill(); // 当たった弾は必ず消す

        // 無敵状態でなければ被弾ダメージ処理を行う
        if (!isInvincible) {
          hitCount_++;
          damageEffectStrength_ = 1.0f;
          MyGame::SetPostEffectMode(PostProcessManager::kModeGrayscale);

          // 3回ヒットしたらゲームオーバー演出開始
          if (hitCount_ >= kMaxHits) {
            phase_ = Phase::GameOverVignette;
            phaseTimer_ = 0.0f;
            vignetteScale_ = 16.0f;
            vignetteExponent_ = 0.8f;
            damageEffectStrength_ = 0.0f;

            MyGame::SetPostEffectMode(PostProcessManager::kModeVignette);
            MyGame::SetPostEffectStrength(1.0f);
            PostProcessManager::SetVignetteParams(vignetteScale_,
                                                  vignetteExponent_);
            projectiles_.clear();

            return; // ここでUpdateを抜けてゲームを一時停止させる
          }
        }
      }
    }
  }

  // グレースケールダメージエフェクトの減衰（プレイ中のみ）
  if (damageEffectStrength_ > 0.0f) {
    damageEffectStrength_ -= 0.01f;
    if (damageEffectStrength_ < 0.0f) {
      damageEffectStrength_ = 0.0f;
      // 爆風もノイズも動作していない場合のみ通常画面に戻す
      if (radialBlurTimer_ <= 0.0f && randomNoiseTimer_ <= 0.0f) {
        MyGame::SetPostEffectMode(PostProcessManager::kModeCopy);
      }
    }
  }
  MyGame::SetPostEffectStrength(damageEffectStrength_);

  // Radial Blur爆風エフェクトの減衰と更新
  if (radialBlurTimer_ > 0.0f) {
    radialBlurTimer_ -= kDeltaTime;
    if (radialBlurTimer_ <= 0.0f) {
      radialBlurTimer_ = 0.0f;
      radialBlurStrength_ = 0.0f;
      // 爆風終了後、ノイズがまだ残っていればノイズに、そうでなければ被弾グレースケール、あるいは通常へ
      if (randomNoiseTimer_ > 0.0f) {
        MyGame::SetPostEffectMode(PostProcessManager::kModeRandom);
      } else if (damageEffectStrength_ > 0.0f) {
        MyGame::SetPostEffectMode(PostProcessManager::kModeGrayscale);
      } else {
        MyGame::SetPostEffectMode(PostProcessManager::kModeCopy);
      }
    } else {
      MyGame::SetPostEffectMode(PostProcessManager::kModeRadialBlur);
      float t = radialBlurTimer_ / kRadialBlurDuration;
      float currentStrength = radialBlurStrength_ * (t * t);
      PostProcessManager::SetRadialBlurParams(0.5f, 0.5f, currentStrength, 12);
    }
  }

  // Randomグリッチノイズエフェクトの減衰と更新
  if (randomNoiseTimer_ > 0.0f) {
    randomNoiseTimer_ -= kDeltaTime;
    if (randomNoiseTimer_ <= 0.0f) {
      randomNoiseTimer_ = 0.0f;
      randomNoiseStrength_ = 0.0f;
      // ノイズ終了後、被弾中ならグレースケールに戻し、そうでなければ通常(Copy)に戻す
      if (damageEffectStrength_ > 0.0f) {
        MyGame::SetPostEffectMode(PostProcessManager::kModeGrayscale);
      } else {
        MyGame::SetPostEffectMode(PostProcessManager::kModeCopy);
      }
    } else {
      // 爆風作動中でない場合のみ、ノイズモードをバインド（爆風優先）
      if (radialBlurTimer_ <= 0.0f) {
        MyGame::SetPostEffectMode(PostProcessManager::kModeRandom);
        float t = randomNoiseTimer_ / 5.0f; // 5秒のジャミングノイズ比率
        if (t > 1.0f) t = 1.0f;
        float easeT = t * t; // イージング（時間の2乗）
        PostProcessManager::SetRandomParams(randomNoiseStrength_ * easeT);
      }
    }
  }

  // 死んだプロジェクタイルを削除
  projectiles_.erase(
      std::remove_if(projectiles_.begin(), projectiles_.end(),
                     [](const std::unique_ptr<EnemyProjectile> &p) {
                       return p->IsDead();
                     }),
      projectiles_.end());

  // スプライト更新（照準をマウス位置に合わせる）
  POINT mousePos;
  GetCursorPos(&mousePos);
  ScreenToClient(Win32Window::GetInstance()->GetHwnd(), &mousePos);
  crosshair_->SetTranslate({static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)});
  crosshair_->Update();
  camera_->Update();

  // 射撃（クリック）処理
  if (!isCovering_ && (ammo_ > 0 || isDebugInfiniteAmmo_) &&
      Input::GetInstance()->IsMouseButtonTriggered(0)) {
    if (!isDebugInfiniteAmmo_) {
      ammo_--; // 弾数を減らす
    }

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

    if (auto *emitter =
            ParticleManager::GetInstance()->GetEmitter("AmmoSparkGroup")) {
      emitter->isPlaying = true;
    }
    ParticleManager::GetInstance()->Emit("AmmoSparkGroup", spawnPos, 1);

    float x = static_cast<float>(mousePos.x) / static_cast<float>(Win32Window::kClientWidth) * 2.0f - 1.0f;
    float y = 1.0f - static_cast<float>(mousePos.y) / static_cast<float>(Win32Window::kClientHeight) * 2.0f;
    Matrix4x4 vp = camera_->GetViewProjectionMatrix();
    Matrix4x4 invVP = Inverse(vp);
    Vector3 nearPos = TransformPoint({x, y, 0.0f}, invVP);
    Vector3 farPos = TransformPoint({x, y, 1.0f}, invVP);
    Vector3 rayDir = Normalize(Subtract(farPos, nearPos));

    // スウォーム群体への射撃コマンドを発行 (GPU判定)
    if (swarmManager_) {
      hasPendingSwarmAttack_ = true;
      pendingSwarmAttack_.rayOrigin = { nearPos.x, nearPos.y, nearPos.z };
      pendingSwarmAttack_.rayDirection = { rayDir.x, rayDir.y, rayDir.z };
      pendingSwarmAttack_.rayRadius = 2.0f;
      pendingSwarmAttack_.damage = 10.0f;
    }

    // 最も手前（tが最小）にあるオブジェクトを判定（貫通防止）
    float closestT = (std::numeric_limits<float>::max)();
    enum class HitTargetType { None, Enemy, BlastProjectile };
    HitTargetType hitType = HitTargetType::None;
    EnemyInfo* hitEnemy = nullptr;
    EnemyProjectile* hitProjectile = nullptr;

    // 1. 爆発弾との当たり判定
    for (auto &p : projectiles_) {
      if (p->IsDead() || !p->IsExplosive())
        continue;

      Vector3 projPos = p->GetPosition();
      Vector3 toProj = Subtract(projPos, nearPos);
      float t = Dot(toProj, rayDir);
      if (t > 0.0f && t < closestT) {
        Vector3 closestPoint = Add(nearPos, Multiply(t, rayDir));
        float dist = Length(Subtract(projPos, closestPoint));
        // 弾の当たり判定半径（少し狙いやすく1.2倍）
        if (dist < p->GetRadius() * 1.2f) {
          closestT = t;
          hitType = HitTargetType::BlastProjectile;
          hitProjectile = p.get();
        }
      }
    }

    // 2. エネミーとの当たり判定
    for (auto &enemy : enemies_) {
      if (!enemy.isActive || enemy.isDead)
        continue;

      Vector3 enemyPos = enemy.object->GetTranslate();
      Vector3 toEnemy = Subtract(enemyPos, nearPos);
      float t = Dot(toEnemy, rayDir);
      if (t > 0.0f && t < closestT) {
        Vector3 closestPoint = Add(nearPos, Multiply(t, rayDir));
        float dist = Length(Subtract(enemyPos, closestPoint));
        if (dist < 1.0f) {
          closestT = t;
          hitType = HitTargetType::Enemy;
          hitEnemy = &enemy;
        }
      }
    }

    // 3. 最も手前のターゲットにのみヒット処理（奥への貫通を防止）
    if (hitType == HitTargetType::BlastProjectile && hitProjectile) {
      Vector3 pPos = hitProjectile->GetPosition();
      bool isDestroyed = hitProjectile->ApplyDamage(1);
      if (isDestroyed) {
        // 爆発弾の破壊エフェクト
        if (auto *emitter = ParticleManager::GetInstance()->GetEmitter("SparkGroup")) {
          emitter->isPlaying = true;
        }
        ParticleManager::GetInstance()->Emit("SparkGroup", pPos, 24);
        score_ += 5; // 迎撃スコア
      } else {
        // 被弾火花エフェクト
        if (auto *emitter = ParticleManager::GetInstance()->GetEmitter("AmmoSparkGroup")) {
          emitter->isPlaying = true;
        }
        ParticleManager::GetInstance()->Emit("AmmoSparkGroup", pPos, 6);
      }
    } else if (hitType == HitTargetType::Enemy && hitEnemy) {
      hitEnemy->isDead = true;
      hitEnemy->isActive = false;
      Vector3 enemyPos = hitEnemy->object->GetTranslate();
      score_ += 10;

      // 敵のインデックスに応じて再生する撃破エフェクトを決定
      std::string effectName =
          ringParticleGroupName_; // デフォルトは1体目のリング
      if (!enemies_.empty()) {
        size_t idx = hitEnemy - &enemies_[0];
        if (idx % 3 == 1) {
          effectName = "CylinderGroup"; // 2体目：シリンダー
        } else if (idx % 3 == 2) {
          effectName = "SparkGroup"; // 3体目：火花
        }
      }

      // 敵撃破時にパーティクルを放出
      if (auto *emitter =
              ParticleManager::GetInstance()->GetEmitter(effectName)) {
        emitter->isPlaying = true;
      }
      ParticleManager::GetInstance()->Emit(effectName, enemyPos, 32);
    }
  }

  // クリア判定
  if (cameraProgress_ >= maxProgress_) {
    bool allEnemiesDead = true;
    for (const auto &enemy : enemies_) {
      if (!enemy.isDead) {
        allEnemiesDead = false;
        break;
      }
    }

    // 撃破エフェクトや完了エフェクトが完全に消えるのを待つ
    bool isEffectFinished = true;
    std::vector<std::string> clearWaitEffects = {ringParticleGroupName_,
                                                 "CylinderGroup", "SparkGroup",
                                                 "ReloadCompleteGroup"};
    for (const auto &name : clearWaitEffects) {
      if (auto *emitter = ParticleManager::GetInstance()->GetEmitter(name)) {
        if (emitter->isPlaying) {
          isEffectFinished = false;
          break;
        }
      }
    }

    if (allEnemiesDead && isEffectFinished) {
      if (phase_ == Phase::Playing) {
        // レール終点でクリア条件達成時: 約100機のスウォームをプレイヤー周辺20m以内にスポーン！
        phase_ = Phase::SwarmBattle;
        swarmBattleTimer_ = 0.0f;
        Vector3 camPos = camera_->GetTranslate();
        if (swarmManager_) {
          swarmManager_->SpawnDrones(100, { camPos.x, camPos.y, camPos.z }, 20.0f);
        }
      }
    }
  }

  // スウォーム（空中群体制御）のシミュレーション更新 (Compute Shader)
  if (swarmManager_) {
    const float kDeltaTime = 1.0f / 60.0f;
    Vector3 camPos = camera_->GetTranslate();
    Vector3 playerPos = { camPos.x, camPos.y, camPos.z };
    if (hasPendingSwarmAttack_) {
      swarmManager_->Update(kDeltaTime, playerPos, camera_.get(), &pendingSwarmAttack_);
      hasPendingSwarmAttack_ = false;
    } else {
      swarmManager_->Update(kDeltaTime, playerPos, camera_.get(), nullptr);
    }
  }

  // -----------------------------------------------------------------
  // UIスプライトの更新処理
  // -----------------------------------------------------------------
  // 1. ライフUIの更新
  {
    int currentLife = kMaxHits - hitCount_;
    if (currentLife < 0)
      currentLife = 0;

    for (int i = 0; i < currentLife; ++i) {
      lifeTargetX_[i] = 1214.0f - (currentLife - 1 - i) * 24.0f;
      lifeCurrentX_[i] += (lifeTargetX_[i] - lifeCurrentX_[i]) * 0.15f;
      lifeUnits_[i]->SetTranslate({lifeCurrentX_[i], 37.0f});
      lifeUnits_[i]->Update();
    }
    lifeBg_->Update();
  }

  // 2. 弾薬UIの更新
  {
    // リロード判定
    bool isReloading = isCovering_ && (ammo_ < kMaxAmmo);

    if (isReloading) {
      // リロード中の補充アニメーション（全体の85%の時間で装填を完了させ、スライドの余裕を作る）
      float t = reloadTimer_ / (kReloadDuration * 0.85f);
      if (t > 1.0f)
        t = 1.0f;
      int reloadCount = static_cast<int>(t * kMaxAmmo);

      float blinkAlpha = 0.4f + 0.6f * std::abs(std::sin(reloadTimer_ * 20.0f));

      // 新しく装填された弾の現在位置を右端(装填口)にする
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
          // 装填済みの弾
          ammoTargetX_[i] = 202.0f - (i + reloadCount - 9) * 20.0f;
          ammoCurrentX_[i] += (ammoTargetX_[i] - ammoCurrentX_[i]) * 0.15f;
          ammoUnits_[i]->SetTranslate({ammoCurrentX_[i], 648.0f});
          ammoUnits_[i]->SetColor(
              {183.0f / 255.0f, 132.0f / 255.0f, 48.0f / 255.0f, blinkAlpha});
        } else {
          // まだ装填されていない弾（非表示）
          ammoUnits_[i]->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
        }
        ammoUnits_[i]->Update();
      }
    } else {
      // 通常時
      lastReloadCount_ = 0; // 通常時はカウントをリセット

      int currentAmmo = ammo_;
      for (int i = 0; i < kMaxAmmo; ++i) {
        if (i >= 9 - currentAmmo) {
          // 残っている弾
          ammoTargetX_[i] = 202.0f - (i - 9 + currentAmmo) * 20.0f;
          ammoCurrentX_[i] += (ammoTargetX_[i] - ammoCurrentX_[i]) * 0.15f;
          ammoUnits_[i]->SetTranslate({ammoCurrentX_[i], 648.0f});
          ammoUnits_[i]->SetColor(
              {183.0f / 255.0f, 132.0f / 255.0f, 48.0f / 255.0f, 1.0f});
        } else {
          // 消費された弾（非表示）
          ammoUnits_[i]->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
        }
        ammoUnits_[i]->Update();
      }
    }
    ammoBg_->Update();
  }

  // 3. プログレスバーの更新
  {
    float progressRatio = cameraProgress_ / maxProgress_;
    if (progressRatio > 1.0f)
      progressRatio = 1.0f;
    progressBar_->SetScale({300.0f * progressRatio, 20.0f});
    progressBar_->Update();
    progressBg_->Update();
  }

  // 4. カバー演出の更新
  if (isCovering_) {
    coverOverlay_->Update();
  }

  // 5. スコアUIの更新
  {
    int tempScore = score_;
    if (tempScore > 9999) tempScore = 9999;
    if (tempScore < 0) tempScore = 0;

    int digits[4];
    digits[0] = tempScore / 1000;
    digits[1] = (tempScore % 1000) / 100;
    digits[2] = (tempScore % 100) / 10;
    digits[3] = tempScore % 10;

    for (int i = 0; i < 4; ++i) {
      if (scoreUnits_[i]) {
        scoreUnits_[i]->SetTexture("numbers/" + std::to_string(digits[i]) + ".png");
        scoreUnits_[i]->SetScale({20.0f, 32.0f});
        scoreUnits_[i]->Update();
      }
    }
    if (scoreBg_) {
      scoreBg_->Update();
    }
  }

  // 6. タイマーUIの更新
  {
    int tempTimer = static_cast<int>(std::round(gameTimer_ * 100.0f));
    if (tempTimer > 9999) tempTimer = 9999;
    if (tempTimer < 0) tempTimer = 0;

    int tDigits[4];
    tDigits[0] = tempTimer / 1000;
    tDigits[1] = (tempTimer % 1000) / 100;
    tDigits[2] = (tempTimer % 100) / 10;
    tDigits[3] = tempTimer % 10;

    for (int i = 0; i < 4; ++i) {
      if (timerUnits_[i]) {
        timerUnits_[i]->SetTexture("numbers/" + std::to_string(tDigits[i]) + ".png");
        timerUnits_[i]->SetScale({20.0f, 32.0f});
        timerUnits_[i]->Update();
      }
    }
    if (timerDot_) {
      timerDot_->Update();
    }
    if (timerBg_) {
      timerBg_->Update();
    }
  }

  // パーティクルの更新
  ParticleManager::GetInstance()->SetIsUpdate(true);
  ParticleManager::GetInstance()->SetUseBillboard(false);
  ParticleManager::GetInstance()->Update(*camera_, kDeltaTime);
}

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
    UpdateImGui_Object3d();
    UpdateImGui_Skybox();
  }
  ImGui::End();

  UpdateImGui_GameStatus();
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
    // ヒットポイントのパラメータを確認
    ImGui::Text("Hit Count: %d / %d", hitCount_, kMaxHits);
    if (ImGui::Button("Reset Hit Count")) {
      hitCount_ = 0;
    }
    if (hitCount_ < kMaxHits) {
      ImGui::Text("Player is Alive");
    } else {
      ImGui::Text("Player is Dead");
    }
    ImGui::Separator();
    ImGui::Text("Ammo: %d / %d", ammo_, kMaxAmmo);
    ImGui::Text("Covering (SPACE): %s",
                isCovering_ ? "YES (Invincible)" : "NO");
    if (isCovering_ && ammo_ < kMaxAmmo) {
      ImGui::ProgressBar(reloadTimer_ / kReloadDuration, ImVec2(0.0f, 0.0f),
                         "Reloading...");
    }
    ImGui::Checkbox("Debug Pause", &isDebugPaused_);
    ImGui::Checkbox("Debug Infinite Ammo", &isDebugInfiniteAmmo_);
    ImGui::Checkbox("Debug Invincible", &isDebugInvincible_);
    if (sectionJumpInvincibleTimer_ > 0.0f) {
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                         "Jump Invincible: %.2fs", sectionJumpInvincibleTimer_);
    }
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
      ImGui::Separator();
    }

    // --- シーンを初期化（論理リセット） ---
    if (ImGui::Button("Reset Game")) {
      hitCount_ = 0;
      orbitAngle_ = 0.0f;
      targetTimer_ = 0.0f;
      cameraYaw_ = 0.0f;
      isHit_ = false;
      damageEffectStrength_ = 0.0f;
      projectileSpawnTimer_ = 0.0f;
      isCovering_ = false;
      coverYOffset_ = 0.0f;
      isPaused_ = false;
      projectiles_.clear();
      score_ = 0;
      gameTimer_ = 60.0f;
      radialBlurStrength_ = 0.0f;
      radialBlurTimer_ = 0.0f;
      randomNoiseStrength_ = 0.0f;
      randomNoiseTimer_ = 0.0f;

      // 敵の論理リセット（モデルのリロードは行わない）
      for (auto &enemy : enemies_) {
        enemy.isDead = false;
        enemy.isActive = false;
        enemy.shootTimer = 0.0f;
        enemy.shootCount = 0;
        enemy.hitShotCount = 0;
        enemy.spawnTimer = 0.0f;
        enemy.object->SetTranslate(enemy.basePosition);
        enemy.object->SetRotation(enemy.baseRotation);
        // 敵のタイプに応じたモデルカラーの再適用
        if (enemy.model) {
          if (enemy.type == EnemyType::Engineer) {
            enemy.model->SetColor({1.0f, 0.8f, 0.2f, 1.0f});
          } else {
            enemy.model->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
          }
        }
        // 初期状態として完全に消去された状態（Threshold = 1.0f）を設定
        enemy.model->SetDissolveParams(1, 1.0f, 0.05f,
                                       Vector3(1.0f, 0.4f, 0.3f));
        enemy.object->Update(mainViewIndex_, camera_.get());
      }

      ammo_ = kMaxAmmo;
      isCovering_ = false;
      reloadTimer_ = 0.0f;

      // UIのイージング座標を強制初期化（ちらつき防止）
      int currentLife = kMaxHits - hitCount_;
      for (int i = 0; i < kMaxHits; ++i) {
        float initX = 1214.0f - (kMaxHits - 1 - i) * 24.0f;
        lifeCurrentX_[i] = initX;
        lifeTargetX_[i] = initX;
        if (i < currentLife) {
          lifeUnits_[i]->SetTranslate({initX, 37.0f});
          lifeUnits_[i]->Update();
        }
      }

      for (int i = 0; i < kMaxAmmo; ++i) {
        float initX = 202.0f - i * 20.0f;
        ammoCurrentX_[i] = initX;
        ammoTargetX_[i] = initX;
        ammoUnits_[i]->SetTranslate({initX, 648.0f});
        ammoUnits_[i]->SetColor(
            {183.0f / 255.0f, 132.0f / 255.0f, 48.0f / 255.0f, 1.0f});
        ammoUnits_[i]->Update();
      }

      // カメラを初期位置に戻す
      cameraProgress_ = 0.0f;
      cameraShakeTimer_ = 0.0f;
      isMovementPaused_ = false;
      Vector3 startCamPos = CalculateRailPosition(cameraProgress_);
      camera_->SetTranslate(startCamPos);
      if (levelData_ && !levelData_->players.empty()) {
        const auto &spawn = levelData_->players[0];
        camera_->SetRotate(spawn.rotation);
        cameraBasePitch_ = spawn.rotation.x;
        cameraYaw_ = spawn.rotation.y;
        cameraBaseRoll_ = spawn.rotation.z;
      } else {
        camera_->SetRotate({0.0f, 0.0f, 0.0f});
        cameraBasePitch_ = 0.0f;
        cameraYaw_ = 0.0f;
        cameraBaseRoll_ = 0.0f;
      }

      // 残留パーティクルを完全にクリア
      ParticleManager::GetInstance()->ClearAllParticles();
      // スムージングを最大限（31）適用してリスタート
      smoothingKernel_ = 31.0f;
      PostProcessManager::SetMode(PostProcessManager::kModeSmoothing);
      PostProcessManager::SetSmoothingParams(
          static_cast<int>(smoothingKernel_));
      phase_ = Phase::RestartSmoothing;
      phaseTimer_ = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Go to Clear Scene")) {
      ClearScene::SetResultData(score_, gameTimer_);
      SceneManager::GetInstance()->ChangeScene("CLEAR");
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
void ShootingScene::UpdateImGui_Object3d() {
  // エネミーのパラメータを調整
  if (ImGui::TreeNode("Enemies Status")) {
    ImGui::Text("Movement Paused: %s", isMovementPaused_ ? "True" : "False");
    ImGui::Text("Progress: %.2f / %.2f", cameraProgress_, maxProgress_);

    for (size_t i = 0; i < enemies_.size(); ++i) {
      auto &enemy = enemies_[i];
      const char* enemyTypeNames[] = { "Normal", "Engineer" };
      int typeInt = static_cast<int>(enemy.type);
      ImGui::Separator();
      if (ImGui::Combo(("Enemy " + std::to_string(i) + " Type").c_str(), &typeInt, enemyTypeNames, IM_ARRAYSIZE(enemyTypeNames))) {
        enemy.type = static_cast<EnemyType>(typeInt);
        if (enemy.model) {
          if (enemy.type == EnemyType::Engineer) {
            enemy.model->SetColor({1.0f, 0.8f, 0.2f, 1.0f});
          } else {
            enemy.model->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
          }
        }
      }
      ImGui::Text("[%zu] Dist: %.1f, Active: %s, Dead: %s", i, enemy.distance,
                  enemy.isActive ? "Yes" : "No", enemy.isDead ? "Yes" : "No");
      ImGui::DragFloat3(("Enemy " + std::to_string(i) + " Position").c_str(),
                        &enemy.object->GetTransformDebug().translate.x, 0.1f);
      ImGui::DragFloat3(("Enemy " + std::to_string(i) + " Rotation").c_str(),
                        &enemy.object->GetTransformDebug().rotate.x, 0.01f);
      ImGui::DragFloat3(("Enemy " + std::to_string(i) + " Scale").c_str(),
                        &enemy.object->GetTransformDebug().scale.x, 0.1f);
    }
    ImGui::TreePop();
  }
  ImGui::Separator();
  if (swarmManager_ && ImGui::TreeNode("Swarm System (空中群体)")) {
    auto& settings = swarmManager_->GetSettings();
    ImGui::Text("Active: %s", swarmManager_->IsActive() ? "YES" : "NO");
    ImGui::Text("Alive / Total: %u / %u", swarmManager_->GetAliveDroneCount(), swarmManager_->GetDroneCount());

    if (ImGui::Button("Spawn Swarm (100 drones, 20m around player)")) {
      Vector3 camPos = camera_->GetTranslate();
      swarmManager_->SpawnDrones(100, { camPos.x, camPos.y, camPos.z }, 20.0f);
      phase_ = Phase::SwarmBattle;
      swarmBattleTimer_ = 0.0f;
    }
    if (ImGui::Button("Spawn in Front of Camera (50 drones, 15m ahead)")) {
      Vector3 camPos = camera_->GetTranslate();
      Vector3 spawnCenter = { camPos.x, camPos.y, camPos.z + 15.0f };
      swarmManager_->SpawnDrones(50, { spawnCenter.x, spawnCenter.y, spawnCenter.z }, 8.0f);
      phase_ = Phase::SwarmBattle;
      swarmBattleTimer_ = 0.0f;
    }

    ImGui::SliderFloat("Drone Model Scale", &swarmManager_->GetDroneScale(), 0.2f, 5.0f, "%.2f");
    ImGui::SliderFloat("Max Speed", &settings.maxSpeed, 1.0f, 40.0f, "%.1f");
    ImGui::SliderFloat("Charge Speed", &settings.kamikazeSpeed, 5.0f, 60.0f, "%.1f");
    ImGui::SliderFloat("Attack Distance", &settings.attackDistance, 5.0f, 60.0f, "%.1f");
    ImGui::Separator();
    ImGui::SliderFloat("Neighbor Radius", &settings.neighborRadius, 2.0f, 30.0f, "%.1f");
    ImGui::SliderFloat("Separation Dist", &settings.separationDist, 0.5f, 10.0f, "%.1f");
    ImGui::SliderFloat("Separation Weight", &settings.separationWeight, 0.0f, 10.0f, "%.2f");
    ImGui::SliderFloat("Alignment Weight", &settings.alignmentWeight, 0.0f, 5.0f, "%.2f");
    ImGui::SliderFloat("Cohesion Weight", &settings.cohesionWeight, 0.0f, 5.0f, "%.2f");
    ImGui::SliderFloat("Target Weight", &settings.targetWeight, 0.0f, 5.0f, "%.2f");
    ImGui::Separator();
    int maxRespawn = static_cast<int>(swarmManager_->GetMaxRespawnCount());
    if (ImGui::DragInt("Max Respawn Count", &maxRespawn, 10, 0, 5000)) {
      swarmManager_->SetMaxRespawnCount(static_cast<uint32_t>(maxRespawn));
    }

    // 各ドローン（先頭15機）の詳細状態表示
    if (ImGui::TreeNode("Individual Drone Status (First 15)")) {
      const auto& drones = swarmManager_->GetDronesData();
      size_t displayCount = (drones.size() < 15) ? drones.size() : 15;
      for (size_t i = 0; i < displayCount; ++i) {
        const auto& d = drones[i];
        const char* stateStr = "Cruise";
        if (d.state == 1) stateStr = "Charge";
        else if (d.state == 2) stateStr = "Dead";

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
    ImGui::SliderFloat("Hit Shot Ratio (必中弾割合)", &hitShotRate_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Miss Shot Spread (演出弾の散布半径)", &missShotSpread_, 1.5f, 8.0f, "%.1f");
    ImGui::DragFloat("Normal Bullet Speed", &speed_, 0.02f, 0.1f, 3.0f);
    ImGui::DragFloat("Engineer Bullet Speed", &engineerProjectileSpeed_, 0.02f, 0.05f, 2.0f);
    ImGui::Separator();
    ImGui::Text("Active Projectiles: %zu", projectiles_.size());
    for (size_t i = 0; i < projectiles_.size(); ++i) {
      auto &p = projectiles_[i];
      Vector3 pos = p->GetPosition();
      ImGui::Text("[%zu] Pos: (%.2f, %.2f, %.2f), Dead: %s", i, pos.x, pos.y,
                  pos.z, p->IsDead() ? "Yes" : "No");
      ImGui::DragFloat3(
          ("Projectile " + std::to_string(i) + " Position").c_str(),
          &p->GetObjectDebug().GetTransformDebug().translate.x, 0.1f);
      ImGui::DragFloat3(
          ("Projectile " + std::to_string(i) + " Rotation").c_str(),
          &p->GetObjectDebug().GetTransformDebug().rotate.x, 0.01f);
      ImGui::DragFloat3(("Projectile " + std::to_string(i) + " Scale").c_str(),
                        &p->GetObjectDebug().GetTransformDebug().scale.x, 0.1f);
      ImGui::DragFloat(("Projectile " + std::to_string(i) + " Speed").c_str(),
                       &speed_, 0.1f);
    }
    ImGui::TreePop();
  }
  ImGui::Separator();
  // フロアオブジェクトのパラメータを調整
  if (ImGui::TreeNode("Floor Object")) {
    if (floorObject_) {
      ImGui::DragFloat3("translate",
                        &floorObject_->GetTransformDebug().translate.x, 0.1f);
      ImGui::DragFloat3("rotate", &floorObject_->GetTransformDebug().rotate.x,
                        0.01f);
      ImGui::DragFloat3("scale", &floorObject_->GetTransformDebug().scale.x,
                        0.1f);
    }
    ImGui::TreePop();
  }
}

// ImGuiでSkyboxのパラメータを調整するための関数
void ShootingScene::UpdateImGui_Skybox() {
  ImGui::Separator();
  if (ImGui::TreeNode("Skybox")) {
    if (skybox_) {
      ImGui::DragFloat3("scale", &skybox_->GetScale().x, 0.1f);
      ImGui::DragFloat3("rotate", &skybox_->GetRotate().x, 0.01f);
      ImGui::DragFloat3("translate", &skybox_->GetTranslate().x, 0.1f);
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
  if (phase_ == Phase::GameOverVignette || phase_ == Phase::GameOverWait) {
    if (hitCount_ >= kMaxHits) {
      ImGui::Text("GAME OVER");
    } else {
      ImGui::Text("CLEAR!");
    }
  } else if (phase_ == Phase::RestartSmoothing) {
    ImGui::Text("Restarting...");
  } else if (phase_ == Phase::SwarmBattle) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.2f, 1.0f), "SWARM BATTLE! (%.1f s)", swarmBattleTimer_);
  } else {
    ImGui::ProgressBar(cameraProgress_ / maxProgress_, ImVec2(0.0f, 0.0f),
                       "Progress");
  }
  ImGui::Separator();
  ImGui::Text("Player Status");
  ImGui::Separator();
  ImGui::Text("Hit Count: %d / %d", hitCount_, kMaxHits);
  ImGui::Text("Ammo: %d / %d", ammo_, kMaxAmmo);
  ImGui::Text("Covering (SPACE): %s", isCovering_ ? "YES (Invincible)" : "NO");
  if (isCovering_ && ammo_ < kMaxAmmo) {
    ImGui::ProgressBar(reloadTimer_ / kReloadDuration, ImVec2(0.0f, 0.0f),
                       "Reloading...");
  }
  ImGui::Separator();
  ImGui::Text("Control");
  ImGui::Separator();
  ImGui::Text("Mouse: Aim");
  ImGui::Text("LMB: Shoot");
  ImGui::Text("SPACE: Cover (Reload)");
  ImGui::End();
}
#endif // USE_IMGUI

void ShootingScene::Draw() {
  // 描画設定
  Object3dCommon::GetInstance()->SetCommonDrawSettings(
      static_cast<BlendState>(currentBlendMode_));

  // メインカメラ (ViewIndex 0)
  // 1. 床オブジェクトの描画
  if (isShowMaterial_ && floorObject_)
    floorObject_->Draw(0);

  // 2. 敵オブジェクトの描画
  if (isShowMaterial_) {
    for (auto &enemy : enemies_) {
      if (enemy.isActive && !enemy.isDead) {
        enemy.object->Draw(0);
      }
    }
  }

  // 3. 敵弾オブジェクトの描画
  for (auto &p : projectiles_)
    p->Draw(0);

  // 3.5 スウォーム群体（空中ドローン群）のGPU Instancing描画
  if (swarmManager_) {
    swarmManager_->Draw(camera_.get());
  }

  // 4.
  // 背景スカイボックスの描画（描画状態切り替えによる競合を防ぐため、常に不透明3Dの後に描画する）
  if (isShowSkybox_)
    skybox_->Draw(0);

  // パーティクルの描画 (加算合成)
  ParticleManager::GetInstance()->Draw(BlendMode::BlendState::kBlendModeAdd);

  // スプライトの描画
  SpriteCommon::GetInstance()->SetCommonDrawSettings(
      static_cast<BlendState>(currentBlendMode_));

  // 1. カバー演出
  if (isCovering_ && coverOverlay_) {
    coverOverlay_->Draw();
  }

  // 2. UIの下地
  if (lifeBg_)
    lifeBg_->Draw();
  if (ammoBg_)
    ammoBg_->Draw();
  if (progressBg_)
    progressBg_->Draw();
  if (scoreBg_)
    scoreBg_->Draw();
  if (timerBg_)
    timerBg_->Draw();

  // 3. UIのゲージ（メモリやバー）
  // ライフメモリ
  {
    int currentLife = kMaxHits - hitCount_;
    if (currentLife < 0)
      currentLife = 0;
    for (int i = 0; i < currentLife; ++i) {
      if (lifeUnits_[i]) {
        lifeUnits_[i]->Draw();
      }
    }
  }

  // 弾薬メモリ
  {
    for (int i = 0; i < kMaxAmmo; ++i) {
      if (ammoUnits_[i]) {
        ammoUnits_[i]->Draw();
      }
    }
  }

  // プログレスバー
  if (progressBar_) {
    progressBar_->Draw();
  }

  // スコア表示スプライト
  for (int i = 0; i < 4; ++i) {
    if (scoreUnits_[i]) {
      scoreUnits_[i]->Draw();
    }
  }

  // タイマー表示スプライト
  for (int i = 0; i < 4; ++i) {
    if (timerUnits_[i]) {
      timerUnits_[i]->Draw();
    }
  }
  if (timerDot_) {
    timerDot_->Draw();
  }

  // 4. 照準
  if (isShowSprite_) {
    if (crosshair_)
      crosshair_->Draw();
  }
}

void ShootingScene::Finalize() {
  Object3dCommon::GetInstance()->SetDefaultCamera(nullptr);
  enemies_.clear();
  floorObject_.reset();
  skybox_.reset();
  projectiles_.clear();
  ParticleManager::GetInstance()->ClearAllParticles();
  PostProcessManager::SetMode(PostProcessManager::kModeCopy);
}

Vector3 ShootingScene::CalculateRailPosition(float progress) {
  if (railPoints_.empty()) {
    return {0.0f, 2.0f, -15.0f};
  }

  float p = progress / maxProgress_;
  if (p < 0.0f)
    p = 0.0f;
  if (p > 1.0f)
    p = 1.0f;

  size_t N = railPoints_.size() - 1;
  float scaledP = p * static_cast<float>(N);
  size_t i = static_cast<size_t>(scaledP);
  if (i >= N) {
    i = N - 1;
  }
  float t = scaledP - static_cast<float>(i);
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;

  const auto &p0 = railPoints_[i].co;
  const auto &p1 = railPoints_[i].handleRight;
  const auto &p2 = railPoints_[i + 1].handleLeft;
  const auto &p3 = railPoints_[i + 1].co;

  float u = 1.0f - t;
  float tt = t * t;
  float uu = u * u;
  float uuu = uu * u;
  float ttt = tt * t;

  Vector3 pos;
  pos.x = uuu * p0.x + 3.0f * uu * t * p1.x + 3.0f * u * tt * p2.x + ttt * p3.x;
  pos.y = uuu * p0.y + 3.0f * uu * t * p1.y + 3.0f * u * tt * p2.y + ttt * p3.y;
  pos.z = uuu * p0.z + 3.0f * uu * t * p1.z + 3.0f * u * tt * p2.z + ttt * p3.z;
  return pos;
}

void ShootingScene::JumpToSection(int index) {
  if (index < 0 || index >= static_cast<int>(sectionProgresses_.size()))
    return;

  float targetProgress = sectionProgresses_[index];

  cameraProgress_ = targetProgress;
  currentSectionIndex_ = index;
  isDebugPaused_ = false; // ワープ後はポーズを自動解除
  sectionJumpInvincibleTimer_ =
      kSectionJumpInvincibleDuration; // ワープ後の無敵タイマーをセット

  // 状態の復元（論理リセット）
  hitCount_ = 0;
  orbitAngle_ = 0.0f;
  targetTimer_ = 0.0f;
  cameraBasePitch_ = 0.0f;
  cameraYaw_ = 0.0f;
  cameraBaseRoll_ = 0.0f;
  cameraShakeTimer_ = 0.0f;
  ParticleManager::GetInstance()->ClearAllParticles();
  isHit_ = false;
  damageEffectStrength_ = 0.0f;
  projectileSpawnTimer_ = 0.0f;
  isCovering_ = false;
  coverYOffset_ = 0.0f;
  isPaused_ = false;
  projectiles_.clear();
  score_ = 0;
  gameTimer_ = 60.0f;
  radialBlurStrength_ = 0.0f;
  radialBlurTimer_ = 0.0f;
  randomNoiseStrength_ = 0.0f;
  randomNoiseTimer_ = 0.0f;

  // 敵の状態復元（移動先より手前の敵は撃破済み、以降の敵は復活・未出現）
  for (auto &enemy : enemies_) {
    enemy.shootCount = 0;
    enemy.hitShotCount = 0;
    // 敵のタイプに応じたモデルカラーの再適用
    if (enemy.model) {
      if (enemy.type == EnemyType::Engineer) {
        enemy.model->SetColor({1.0f, 0.8f, 0.2f, 1.0f});
      } else {
        enemy.model->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
      }
    }
    if (enemy.distance < targetProgress) {
      enemy.isDead = true;
      enemy.isActive = false;
      enemy.shootTimer = 0.0f;
      enemy.spawnTimer = 1.0f; // 出現完了状態扱い
      score_ += 10;
      if (enemy.model) {
        enemy.model->SetDissolveParams(0, 0.0f, 0.05f,
                                       Vector3(1.0f, 0.4f, 0.3f));
      }
    } else {
      enemy.isDead = false;
      enemy.isActive = false;
      enemy.shootTimer = 0.0f;
      enemy.spawnTimer = 0.0f;
      enemy.object->SetTranslate(enemy.basePosition);
      enemy.object->SetRotation(enemy.baseRotation);
      if (enemy.model) {
        enemy.model->SetDissolveParams(1, 1.0f, 0.05f,
                                       Vector3(1.0f, 0.4f, 0.3f));
      }
    }
    enemy.object->Update(mainViewIndex_, camera_.get());
  }

  ammo_ = kMaxAmmo;
  isCovering_ = false;
  reloadTimer_ = 0.0f;

  // UIのイージング座標を強制初期化（ちらつき防止）
  int currentLife = kMaxHits - hitCount_;
  for (int i = 0; i < kMaxHits; ++i) {
    float initX = 1214.0f - (kMaxHits - 1 - i) * 24.0f;
    lifeCurrentX_[i] = initX;
    lifeTargetX_[i] = initX;
    if (i < currentLife) {
      lifeUnits_[i]->SetTranslate({initX, 37.0f});
      lifeUnits_[i]->Update();
    }
  }

  for (int i = 0; i < kMaxAmmo; ++i) {
    float initX = 202.0f - i * 20.0f;
    ammoCurrentX_[i] = initX;
    ammoTargetX_[i] = initX;
    ammoUnits_[i]->SetTranslate({initX, 648.0f});
    ammoUnits_[i]->SetColor(
        {183.0f / 255.0f, 132.0f / 255.0f, 48.0f / 255.0f, 1.0f});
    ammoUnits_[i]->Update();
  }

  // カメラを初期位置に戻す
  Vector3 startCamPos = CalculateRailPosition(cameraProgress_);
  camera_->SetTranslate(startCamPos);
  if (levelData_ && !levelData_->players.empty()) {
    const auto &spawn = levelData_->players[0];
    camera_->SetRotate(spawn.rotation);
    cameraYaw_ = spawn.rotation.y;
  } else {
    camera_->SetRotate({0.0f, 0.0f, 0.0f});
    cameraYaw_ = 0.0f;
  }

  // カメラの一時停止フラグ（スタート地点以外は敵出現ポイントなので停止状態から始める）
  isMovementPaused_ = (targetProgress > 0.0f);

  // スムージング演出をリスタート用に設定
  smoothingKernel_ = 31.0f;
  PostProcessManager::SetMode(PostProcessManager::kModeSmoothing);
  PostProcessManager::SetSmoothingParams(static_cast<int>(smoothingKernel_));
  phase_ = Phase::RestartSmoothing;
  phaseTimer_ = 0.0f;
}

void ShootingScene::UpdateRailMovement() {
  // 進行一時停止中なら、画面内の全アクティブエネミーが全滅したかチェック
  if (isMovementPaused_) {
    bool allDead = true;
    for (const auto &enemy : enemies_) {
      if (enemy.isActive && !enemy.isDead) {
        allDead = false;
        break;
      }
    }
    if (allDead) {
      isMovementPaused_ = false;
    }
  }

  // 自動進行処理および敵出現トリガーチェック
  if (!isMovementPaused_) {
    float nextProgress = cameraProgress_ + kCameraSpeed;
    bool triggered = false;
    float triggerDistance = nextProgress;

    // 最も近い出現トリガーを検出
    for (auto &enemy : enemies_) {
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
      // トリガーを引いたエネミーの出現距離にピッタリ吸着させて停止
      cameraProgress_ = triggerDistance;
      isMovementPaused_ = true;
    } else {
      cameraProgress_ = nextProgress;
      if (cameraProgress_ > maxProgress_) {
        cameraProgress_ = maxProgress_;
      }
    }
  }
}

bool ShootingScene::IsInvincible() const {
  // 1. デバッグ常時無敵
  if (isDebugInvincible_) {
    return true;
  }
  // 2. フェーズ移動後の一時無敵
  if (sectionJumpInvincibleTimer_ > 0.0f) {
    return true;
  }
  // 3. カバー中かつ「十分に下がりきっている」状態（Yオフセットが coverYTarget_ 以下）
  if (isCovering_ && coverYOffset_ <= coverYTarget_) {
    return true;
  }

  return false;
}