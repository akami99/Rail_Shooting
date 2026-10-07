#include "TitleScene.h"
#include "ApplicationConfig.h" // kDeltaTimeなど
#include "BlendMode.h"
#include "DX12Context.h"
#include "Win32Window.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "Model.h"
#include "ModelManager.h"
#include "Object3dCommon.h"
#include "ParticleManager.h"
#include "SceneManager.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
// 計算用関数など
#include "MathUtils.h"
#include "MatrixGenerators.h"

// using
using namespace MathUtils;
using namespace MathGenerators;
using namespace BlendMode;

void TitleScene::Initialize()
{
#ifdef USE_IMGUI
    ShowCursor(TRUE);
#else
    ShowCursor(FALSE);
#endif

    // カメラ生成
    camera_ = std::make_unique<Camera>(); // メモリ確保と同時にスマートポインタ化
    camera_->Initialize();
    camera_->SetTranslate(cameraSettings_.translate);
    camera_->SetRotate(cameraSettings_.rotate);
    camera_->SetFovY(cameraSettings_.fovY * (std::numbers::pi_v<float> / 180.0f));

    debugCamera_.Initialize();

    Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());

    // モデル読み込み
    ModelManager::GetInstance()->LoadModel(floorModelDirectory_, floorModelPath_);
    ModelManager::GetInstance()->LoadModel(backWallModelDirectory_, backWallModelPath_);
    ModelManager::GetInstance()->LoadModel(playerCoverModelDirectory_, playerCoverModelPath_);
    ModelManager::GetInstance()->LoadModel(targetCoverModelDirectory_, targetCoverModelPath_);
    ModelManager::GetInstance()->LoadModel(startBoardModelDirectory_, startBoardModelPath_);
    ModelManager::GetInstance()->LoadModel(targetBoardModelDirectory_, targetBoardModelPath_);

    // テクスチャ読み込み
    TextureManager::GetInstance()->LoadTexture(tilePath_);
    TextureManager::GetInstance()->LoadTexture(titleTextPath_);
    TextureManager::GetInstance()->LoadTexture(controlPath_);
    TextureManager::GetInstance()->LoadTexture(crosshairPath_);

    // オブジェクトの初期化
    /// === タイル画像をタイリングするためにUVスケールを設定 ===
    // --- フロアオブジェクト生成 ---
    floorObject_ = std::make_unique<Object3d>();
    floorObject_->Initialize();
    floorObject_->SetModel(floorModelPath_);
    floorObject_->SetOverrideTexturePath(tilePath_);
    floorObject_->SetUvScale(floorObjectSettings_.uvScale_);
    floorObject_->SetTranslate(floorObjectSettings_.position_);
    floorObject_->SetScale(floorObjectSettings_.scale_);
    floorObject_->SetCamera(camera_.get());

    // --- バックウォールオブジェクト生成 ---
    backWallObject_ = std::make_unique<Object3d>();
    backWallObject_->Initialize();
    backWallObject_->SetModel(backWallModelPath_);
    backWallObject_->SetOverrideTexturePath(tilePath_);
    backWallObject_->SetUvScale(backWallObjectSettings_.uvScale_);
    backWallObject_->SetTranslate(backWallObjectSettings_.position_);
    backWallObject_->SetScale(backWallObjectSettings_.scale_);
    backWallObject_->SetCamera(camera_.get());

    // --- プレイヤーカバーオブジェクト生成 ---
    playerCoverObject_ = std::make_unique<Object3d>();
    playerCoverObject_->Initialize();
    playerCoverObject_->SetModel(playerCoverModelPath_);
    playerCoverObject_->SetOverrideTexturePath(tilePath_);
    playerCoverObject_->SetUvScale(playerCoverObjectSettings_.uvScale_);
    playerCoverObject_->SetTranslate(playerCoverObjectSettings_.position_);
    playerCoverObject_->SetCamera(camera_.get());

    // --- ターゲットカバーオブジェクト生成 ---
    for (int i = 0; i < 4; ++i) {
        TargetCoverInfo cover;
        cover.object_ = std::make_unique<Object3d>();
        cover.object_->Initialize();
        cover.object_->SetModel(targetCoverModelPath_);
        cover.object_->SetOverrideTexturePath(tilePath_);
        cover.object_->SetUvScale(cover.uvScale_);
        cover.object_->SetCamera(camera_.get());
        cover.position_ = coverConfigs_[i].pos;
        if (coverConfigs_[i].isVertical) {
            // 縦長: Z軸90度回転、AABBサイズ(幅1.0m, 高さ1.8m, 奥行0.5m)
            cover.rotation_ = kCoverVerticalRotate;
            cover.aabbSize_ = kCoverVerticalSize;
        }
        cover.object_->SetTranslate(cover.position_);
        cover.object_->SetRotation(cover.rotation_);
        targetCovers_.push_back(std::move(cover));
    }
    /// === ここまで ===

    // --- スタートボードオブジェクト生成 ---
    startBoardObject_ = std::make_unique<Object3d>();
    startBoardObject_->Initialize();
    startBoardObject_->SetModel(startBoardModelPath_);
    startBoardObject_->SetTranslate(startBoardObjectSettings_.position_);
    startBoardObject_->SetRotation(startBoardObjectSettings_.rotation_);
    startBoardObject_->SetCamera(camera_.get());

    // --- ターゲットボードオブジェクト生成 ---
    for (int i = 0; i < 4; ++i) {
        TargetBoardInfo info;
        info.targetBoardObject_ = std::make_unique<Object3d>();
        info.targetBoardObject_->Initialize();
        info.targetBoardObject_->SetModel(targetBoardModelPath_);
        info.basePosition_ = configs_[i].pos;
        info.isMoving_ = configs_[i].isMoving;
        info.moveSpeed_ = configs_[i].speed;
        info.moveAmplitude_ = 1.0f; // ±1.0m (計2m往復)
        info.targetBoardObject_->SetTranslate(info.basePosition_);
        info.targetBoardObject_->SetRotation(info.rotation_);
        info.targetBoardObject_->SetCamera(camera_.get());
        targetBoards_.push_back(std::move(info));
    }

    // スプライトの初期化
    // --- タイトルテキストスプライト生成 ---
    titleTextSprite_ = std::make_unique<Sprite>();
    titleTextSprite_->Initialize(titleTextPath_);
    titleTextSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    titleTextSprite_->SetTranslate(titleTextSpriteSettings_.translate_);
    titleTextSprite_->SetScale(titleTextSpriteSettings_.scale_);

    // --- コントロールスプライト生成 ---
    controlSprite_ = std::make_unique<Sprite>();
    controlSprite_->Initialize(controlPath_);
    controlSprite_->SetAnchorPoint({0.5f, 0.5f});
    controlSprite_->SetTranslate(controlSpriteSettings_.translate_);
    controlSprite_->SetScale(controlSpriteSettings_.scale_);

    // --- クロスヘアスプライト生成 ---
    crosshairSprite_ = std::make_unique<Sprite>();
    crosshairSprite_->Initialize(crosshairPath_);
    crosshairSprite_->SetAnchorPoint({0.5f, 0.5f});
    crosshairSprite_->SetColor(crosshairSpriteSettings_.colorNoAim_);

    // TitleScene.cpp (Initialize関数内)
    TextureManager::GetInstance()->LoadTexture("white.png");
    // 敵弾トレイルエフェクトのセットアップ
    {
        std::string groupName = "BulletTrail_Normal";
        ParticleManager::GetInstance()->CreateParticleGroup(groupName, "white.png");
        Transform t = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
        ParticleEmitter emitter(t, 1, 0.1f);
        emitter.isEmit = false;
        emitter.isEffectMode = false;
        emitter.generateSettings.fixedLifeTime = 0.3f;
        emitter.generateSettings.fixedColor = { 0.9f, 0.95f, 1.0f, 0.85f };
        emitter.SetShapeType(ParticleShapeType::Cylinder);
        if (auto *cs = emitter.GetCylinderShape()) {
            cs->settings.height = 0.12f;
            cs->settings.topRadius = { 0.45f, 0.45f };
            cs->settings.bottomRadius = { 0.45f, 0.45f };
            cs->settings.startAngle = 0.0f;
            cs->settings.endAngle = 180.0f; // 半円状
            cs->settings.division = 24;
            cs->settings.verticalDivision = 1;
            cs->settings.topColor = { 0.9f, 0.95f, 1.0f, 0.85f };
            cs->settings.bottomColor = { 0.8f, 0.9f, 1.0f, 0.7f };
            cs->settings.fadeStartAlpha = 1.0f;
            cs->settings.fadeEndAlpha = 1.0f;
            cs->settings.fadeRange = 0.15f; // 両端フェード
            cs->settings.alphaReference = 0.0f;
        }
        ParticleManager::GetInstance()->SetEmitter(groupName, emitter);
    }
}

void TitleScene::Update()
{
    // 入力の更新
    Input::GetInstance()->Update();
    // 3. UI処理 (ImGuiの定義)
    UpdateImGui();

    // カメラの更新処理
    UpdateGameCamera();

    if (Input::GetInstance()->IsKeyTriggered(DIK_RETURN)) {
        SceneManager::GetInstance()->ChangeScene("SHOOTING");
    }

    UpdateCrosshairAndRaycast();

    UpdateEnemyShooting();

    UpdateProjectiles();

    UpdateCover();

    UpdatePlayerShot();

    UpdateTargetsAnimation();

    UpdateStartBoardAnimation();

    // オブジェクトの更新
    // --- フロアオブジェクトの更新 ---
    if (floorObject_) {
        floorObject_->Update();
    }

    // --- バックウォールオブジェクトの更新 ---
    if (backWallObject_) {
        backWallObject_->Update();
    }

    // --- プレイヤーカバーオブジェクトの更新 ---
    if (playerCoverObject_) {
        playerCoverObject_->Update();
    }

    // --- ターゲットカバーオブジェクトの更新 ---
    for (auto &cover : targetCovers_) {
        if (cover.object_) cover.object_->Update();
    }

    // --- スタートボードオブジェクトの更新 ---
    if (startBoardObject_) {
        startBoardObject_->Update();
    }

    // --- ターゲットボードオブジェクトの更新 ---
    for (auto &targetBoardInfo : targetBoards_) {
        if (targetBoardInfo.targetBoardObject_) {
            targetBoardInfo.targetBoardObject_->Update();
        }
    }

    // スプライトの更新
    // --- タイトルテキストスプライトの更新 ---
    if (titleTextSprite_) {
        titleTextSprite_->Update();
    }

    // --- コントロールスプライトの更新 ---
    if (controlSprite_) {
      controlSprite_->Update();
    }

    // --- パーティクルの更新 ---
    ParticleManager::GetInstance()->SetIsUpdate(true);
    ParticleManager::GetInstance()->SetUseBillboard(false); // 3DシリンダーなのでBillboardはOFF
    ParticleManager::GetInstance()->Update(*camera_, kDeltaTime);
}

void TitleScene::Draw()
{
    // オブジェクトの描画
    // 描画設定
    Object3dCommon::GetInstance()->SetCommonDrawSettings(static_cast<BlendState>(currentBlendMode_));

    // フロアオブジェクトを描画
    if (floorObject_) {
      floorObject_->Draw();
    }

    // バックウォールオブジェクトを描画
    if (backWallObject_) {
      backWallObject_->Draw();
    }

    // プレイヤーカバーオブジェクトを描画
    if (playerCoverObject_) {
      playerCoverObject_->Draw();
    }

    // ターゲットカバーオブジェクトを描画
    for (auto &cover : targetCovers_) {
        if (cover.object_) cover.object_->Draw();
    }

    // スタートボードオブジェクトを描画
    if (startBoardObject_) {
      startBoardObject_->Draw();
    }

    // ターゲットボードオブジェクトを描画
    for (auto &targetBoardInfo : targetBoards_) {
      if (targetBoardInfo.targetBoardObject_) {
        targetBoardInfo.targetBoardObject_->Draw();
      }
    }

    // 弾の描画
    for (auto &p : projectiles_) {
        if (!p->IsDead()) {
            p->Draw(0);
        }
    }

    // パーティクルの描画
    ParticleManager::GetInstance()->Draw(BlendMode::BlendState::kBlendModeAdd);

    // スプライトの描画
    // 描画設定
    SpriteCommon::GetInstance()->SetCommonDrawSettings(static_cast<BlendState>(currentBlendMode_));

    // クロスヘアスプライトを描画
    if (crosshairSprite_) {
      crosshairSprite_->Draw();
    }

    // タイトルテキストを描画
    if (titleTextSprite_) {
        titleTextSprite_->Draw();
    }

    // コントロールスプライトを描画
    if (controlSprite_) {
      controlSprite_->Draw();
    }
}

void TitleScene::Finalize()
{
    // Object3dCommonの参照をクリア（次のシーン切り替え前に）
    Object3dCommon::GetInstance()->SetDefaultCamera(nullptr);

    floorObject_.reset();
    backWallObject_.reset();
    playerCoverObject_.reset();
    targetCovers_.clear();
    startBoardObject_.reset();
    targetBoards_.clear();
    titleTextSprite_.reset();
    controlSprite_.reset();
    crosshairSprite_.reset();
    projectiles_.clear();
    // --- パーティクルのクリア ---
    ParticleManager::GetInstance()->ClearAllParticles();
}

void TitleScene::UpdateCrosshairAndRaycast()
{
    // 1. マウス位置取得とクロスヘアスプライトの追従
    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(Win32Window::GetInstance()->GetHwnd(), &mousePos);
    crosshairSprite_->SetTranslate({ static_cast<float>(mousePos.x), static_cast<float>(mousePos.y) });
    crosshairSprite_->Update();
    // 2. マウス座標から3D空間のレイ（光線）を算出
    float x = static_cast<float>(mousePos.x) / static_cast<float>(Win32Window::kClientWidth) * 2.0f - 1.0f;
    float y = 1.0f - static_cast<float>(mousePos.y) / static_cast<float>(Win32Window::kClientHeight) * 2.0f;
    Matrix4x4 invVP = Inverse(camera_->GetViewProjectionMatrix());
    Vector3 nearPos = TransformPoint({ x, y, 0.0f }, invVP);
    Vector3 farPos = TransformPoint({ x, y, 1.0f }, invVP);
    Vector3 rayDir = Normalize(Subtract(farPos, nearPos));
    // 3. 看板・的との交差判定（距離による簡易球判定）
    currentAimTarget_ = TargetType::None;
    // カバー中は狙えない仕様
    if (!isCovering_) {
        float closestT = (std::numeric_limits<float>::max)();
        TargetType closestHit = TargetType::None;
        float t = 0.0f;

        // 敵側カバー (横:幅1.8m, 高さ1.0m, 奥行0.5m, 縦:幅1.0m, 高さ1.8m, 奥行0.5m)
        for (const auto &cover : targetCovers_) {
            if (!cover.object_) continue;
            AABB box = MakeAABB(cover.position_, cover.aabbSize_);
            if (IntersectRayAABB(nearPos, rayDir, box, t) && t < closestT) {
                closestT = t;
                closestHit = TargetType::Cover;
            }
        }
        // START看板との判定 (幅3.0m, 高さ1.5m, 奥行0.3m)
        if (startBoardObject_ && startBoardObjectSettings_.state_ == TargetState::Standing) {
            Vector3 startBoardCenter = startBoardObject_->GetTranslate();
            startBoardCenter.y += startBoardObjectSettings_.aabbSize_.y * 0.5f + 0.75f; // 高さの中心を計算
            AABB box = MakeAABB(startBoardCenter, startBoardObjectSettings_.aabbSize_);
            if (IntersectRayAABB(nearPos, rayDir, box, t) && t < closestT) {
                closestT = t;
                closestHit = TargetType::StartBoard;
            }
        }
        // 的4体の判定 (幅1.0m, 高さ1.5m, 奥行0.3m)
        aimedTargetIndex_ = -1;
        for (size_t i = 0; i < targetBoards_.size(); ++i) {
            if (!targetBoards_[i].targetBoardObject_) continue;
            if (targetBoards_[i].state_ != TargetState::Standing) continue;
            // 現在の座標を取得してAABB判定（移動中も正確に追従）
            Vector3 targetCenter = targetBoards_[i].targetBoardObject_->GetTranslate();
            targetCenter.y += targetBoards_[i].aabbSize_.y * 0.5f; // 高さの中心を計算
            AABB box = MakeAABB(targetCenter, targetBoards_[i].aabbSize_);
            if (IntersectRayAABB(nearPos, rayDir, box, t) && t < closestT) {
                closestT = t;
                closestHit = TargetType::TargetBoard;
                aimedTargetIndex_ = static_cast<int>(i);
            }
        }

        // --- 最終結果の反映 ---
        // 一番手前にあるオブジェクトの種類を代入
        currentAimTarget_ = closestHit;
    }
    // 4. クロスヘアの色変更(狙えている時は赤、外れ or カバー中は緑)
    if (currentAimTarget_ != TargetType::None && currentAimTarget_ != TargetType::Cover) {
        crosshairSprite_->SetColor(crosshairSpriteSettings_.colorAim_); // 赤
    } else {
        crosshairSprite_->SetColor(crosshairSpriteSettings_.colorNoAim_); // 緑
    }
}

void TitleScene::UpdateCover()
{
    // 手動（SPACEキー）または 自動カバー判定 のどちらかが有効ならカバー
    isCovering_ = Input::GetInstance()->IsKeyDown(DIK_SPACE) || isAutoCovering_;
    // カバー目標値へ向けてスムーズにしゃがむ / 立ち上がる
    float targetOffset = isCovering_ ? coverYTarget_ : 0.0f;
    coverYOffset_ += (targetOffset - coverYOffset_) * 0.15f;
    Vector3 camPos = cameraSettings_.translate;
    camPos.y += coverYOffset_;
    camera_->SetTranslate(camPos);
    // UI
    Vector2 titlePos = titleTextSpriteSettings_.translate_;
    titlePos.y += coverYOffset_ * 80.0f;
    titleTextSprite_->SetTranslate(titlePos);
    Vector2 controlPos = controlSpriteSettings_.translate_;
    controlPos.y += coverYOffset_ * 40.0f;
    controlSprite_->SetTranslate(controlPos);
}

void TitleScene::UpdatePlayerShot()
{
    // カバー中でなく、左クリックが押されたら射撃（無限弾薬）
    if (!isCovering_ && Input::GetInstance()->IsMouseButtonTriggered(0)) {
        if (!isCovering_ && Input::GetInstance()->IsMouseButtonTriggered(0)) {
            if (currentAimTarget_ == TargetType::StartBoard) {           // START看板
                startBoardObjectSettings_.state_ = TargetState::Falling;
            } else if (currentAimTarget_ == TargetType::TargetBoard) {   // 的
                if (aimedTargetIndex_ >= 0 && aimedTargetIndex_ < (int)targetBoards_.size()) {
                    targetBoards_[aimedTargetIndex_].state_ = TargetState::Falling;
                }
            }
        }
    }
}

void TitleScene::UpdateTargetsAnimation()
{
    const float maxAngle = std::numbers::pi_v<float> *0.5f; // 90度 (π/2)
    for (auto &target : targetBoards_) {
        // --- 横移動の更新（立っている時のみ移動）---
        if (target.isMoving_ && target.state_ == TargetState::Standing) {
            target.moveTimer_ += kDeltaTime;
            Vector3 pos = target.basePosition_;
            // sin波で左右に ±1.0m 往復
            pos.x += std::sin(target.moveTimer_ * target.moveSpeed_) * target.moveAmplitude_;
            target.targetBoardObject_->SetTranslate(pos);
        }

        // --- 倒れ・復帰アニメーション ---
        switch (target.state_) {
        case TargetState::Standing:
            // 直立維持
            target.fallAngle_ = 0.0f;
            break;
        case TargetState::Falling:
            // 奥へ倒れるアニメーション
            target.fallAngle_ += kTargetFallSpeed_ * kDeltaTime;
            if (target.fallAngle_ >= maxAngle) {
                target.fallAngle_ = maxAngle;
                target.state_ = TargetState::Down;
                target.downTimer_ = 0.0f; // タイマースタート
            }
            break;
        case TargetState::Down:
            // 倒れたまま5秒待機
            target.downTimer_ += kDeltaTime;
            if (target.downTimer_ >= kTargetWaitDuration_) {
                target.state_ = TargetState::Rising; // 起き上がり開始
            }
            break;
        case TargetState::Rising:
            // 手前に起き上がるアニメーション
            target.fallAngle_ -= kTargetRiseSpeed_ * kDeltaTime;
            if (target.fallAngle_ <= 0.0f) {
                target.fallAngle_ = 0.0f;
                target.state_ = TargetState::Standing; // 復帰完了
            }
            break;
        }
        // オブジェクトの回転に反映（X軸回転で奥へ倒す）
        if (target.targetBoardObject_) {
            Vector3 rot = target.rotation_;
            rot.x += target.fallAngle_;
            target.targetBoardObject_->SetRotation(rot);
        }
    }
}

void TitleScene::UpdateStartBoardAnimation()
{
    const float maxAngle = std::numbers::pi_v<float> *0.5f;
    if (startBoardObjectSettings_.state_ == TargetState::Falling) {
        startBoardObjectSettings_.fallAngle_ += kTargetFallSpeed_ * kDeltaTime;
        // 倒れ切ったタイミングでシーン遷移
        if (startBoardObjectSettings_.fallAngle_ >= maxAngle) {
            startBoardObjectSettings_.fallAngle_ = maxAngle;
            startBoardObjectSettings_.state_ = TargetState::Down;
            // シューティングシーンへ遷移！
            SceneManager::GetInstance()->ChangeScene("SHOOTING");
            return;
        }
        // 看板の回転に反映
        if (startBoardObject_) {
            Vector3 rot = startBoardObjectSettings_.rotation_;
            rot.x += startBoardObjectSettings_.fallAngle_;
            startBoardObject_->SetRotation(rot);
        }
    }
}

void TitleScene::UpdateEnemyShooting()
{
    shootTimer_ += kDeltaTime;
    if (shootTimer_ < kShootInterval_) {
        return;
    }
    
    // 的が存在しない場合は処理しない
    if (targetBoards_.empty()) return;

    // 撃てるかどうかの判定用ラムダ式
    auto canShoot = [&](int idx) {
        if (idx < 0 || idx >= (int)targetBoards_.size()) return false;
        // 1. 立っていること
        if (targetBoards_[idx].state_ != TargetState::Standing) return false;
        // 2. 一番左の敵はカバーに隠れていないこと
        if (idx == 0 && IsTargetHiddenByCover(0)) return false;
        return true;
        };

    // 次に撃つ予定の的をチェック
    int targetIdx = nextShootTargetIndex_;
    nextShootTargetIndex_ = (nextShootTargetIndex_ + 1) % targetBoards_.size(); // 次回は逆側
    // 狙う側の的が倒れている場合、もう片方が立っていればそちらに変更
    if (!canShoot(targetIdx)) {
        // 他に撃てる敵を探す
        int foundIdx = -1;
        for (int i = 1; i < (int)targetBoards_.size(); ++i) {
            int candidate = (targetIdx + i) % targetBoards_.size();
            if (canShoot(candidate)) {
                foundIdx = candidate;
                break;
            }
        }
        if (foundIdx == -1) {
            return; // 誰も撃てる状態でないなら今回は発射スキップ
        }
        targetIdx = foundIdx;
    }

    shootTimer_ = 0.0f;

    // 発射位置（的の中心左上から少し前）
    Vector3 spawnPos = targetBoards_[targetIdx].targetBoardObject_->GetTranslate();
    spawnPos += kShootOffset;
    // プレイヤー（カメラ）へ向かう方向ベクトル
    Vector3 playerPos = cameraSettings_.translate;
    Vector3 dir = Normalize(Subtract(playerPos, spawnPos));
    Vector3 velocity = Multiply(kEnemyBulletSpeed_, dir);
    // 弾を生成して追加
    auto projectile = std::make_unique<EnemyProjectile>();
    projectile->Initialize(spawnPos, velocity, EnemyProjectile::Type::Normal);
    projectiles_.push_back(std::move(projectile));
}

void TitleScene::UpdateProjectiles()
{
    Vector3 playerPos = cameraSettings_.translate;
    bool hasApproachingBullet = false;
    for (auto &p : projectiles_) {
        if (p->IsDead()) continue;
        // 弾の更新（移動処理）
        p->Update();
        p->Update(0, camera_.get());
        Vector3 bulletPos = p->GetPosition();
        float distToPlayer = Length(Subtract(playerPos, bulletPos));
        // 1. 自動カバー判定: 弾が一定距離まで近づいたら自動カバー発動
        if (distToPlayer <= kAutoCoverDistance_ && bulletPos.z > playerPos.z) {
            hasApproachingBullet = true;
        }
        // 2. 弾の消滅判定: プレイヤーの手前・遮蔽物を通過（または通り過ぎた）
        // カメラの少し手前（z <= playerPos.z + 0.3f）に達したら着弾・通過とみなして消す
        if (bulletPos.z <= playerPos.z + 0.3f) {
            p->Kill(); // 弾を消滅フラグへ
        }
    }
    // 迫ってくる弾が1つでもあればカバー維持、無くなれば自動カバー解除
    isAutoCovering_ = hasApproachingBullet;
    // 3. 死んだ弾をメモリから削除（クリーンアップ）
    projectiles_.erase(
        std::remove_if(projectiles_.begin(), projectiles_.end(),
            [](const std::unique_ptr<EnemyProjectile> &p) {
                return p->IsDead();
            }),
        projectiles_.end());
}

void TitleScene::UpdateGameCamera()
{

    if (Input::GetInstance()->IsKeyTriggered(DIK_F1)) {
        useDebugCamera_ = !useDebugCamera_;
    }

    if (useDebugCamera_) {
        debugCamera_.Update();
        camera_->SetRotate(debugCamera_.GetRotation());
        camera_->SetTranslate(debugCamera_.GetTranslate());
    }
    camera_->Update();
}

void TitleScene::UpdateImGui()
{
#ifdef USE_IMGUI
    // シーンの表示
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
    ImGui::Begin("Scene");
    ImGui::Text("Title Scene");
    ImGui::Text("Press ENTER or SPACE to start");
    if (ImGui::Button("Start Shooting")) {
        SceneManager::GetInstance()->ChangeScene("SHOOTING");
    }
    ImGui::End();

    // 各種
    ImGui::SetNextWindowPos(ImVec2(1000, 10), ImGuiCond_Once);
    ImGui::Begin("Settings");

    // カメラ
    if (ImGui::TreeNode("Camera")) {
      ImGui::DragFloat3("Position", &cameraSettings_.translate.x, 0.1f);
      ImGui::DragFloat3("Rotation", &cameraSettings_.rotate.x, 0.1f);
      ImGui::DragFloat("FOV Y", &cameraSettings_.fovY, 1.0f, 1.0f, 179.0f);
      if (ImGui::Button("Update Camera")) {
          isUpdateCameraSettings_ = !isUpdateCameraSettings_;
      }
      if (isUpdateCameraSettings_) {
          ImGui::Text("Camera Settings Updated");
          camera_->SetTranslate(cameraSettings_.translate);
          camera_->SetRotate(cameraSettings_.rotate);
          camera_->SetFovY(cameraSettings_.fovY * (std::numbers::pi_v<float> / 180.0f));
      } else {
        ImGui::Text("Camera Settings Not Updated");
      }
      
      ImGui::TreePop();
    }

    // オブジェクト
    if (ImGui::TreeNode("Objects")) {
        // オブジェクトの設定
      if (ImGui::TreeNode("Floor Object")) {
        ImGui::DragFloat3("Position", &floorObjectSettings_.position_.x, 0.1f);
        ImGui::DragFloat3("Scale", &floorObjectSettings_.scale_.x, 0.1f);
        Vector2 uvScale = floorObject_->GetUvScale();
        ImGui::DragFloat2("UV Scale", &uvScale.x, 0.1f);
        if (floorObject_) {
            floorObject_->SetTranslate(floorObjectSettings_.position_);
            floorObject_->SetScale(floorObjectSettings_.scale_);
            floorObject_->SetUvScale(uvScale);
        }

        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Back Wall Object")) {
          ImGui::DragFloat3("Position", &backWallObjectSettings_.position_.x, 0.1f);
          ImGui::DragFloat3("Scale", &backWallObjectSettings_.scale_.x, 0.1f);
          Vector2 uvScale = backWallObject_->GetUvScale();
          ImGui::DragFloat2("UV Scale", &uvScale.x, 0.1f);
          if (backWallObject_) {
              backWallObject_->SetTranslate(backWallObjectSettings_.position_);
              backWallObject_->SetScale(backWallObjectSettings_.scale_);
              backWallObject_->SetUvScale(uvScale);
          }

          ImGui::TreePop();
      }
      if (ImGui::TreeNode("Player Cover Object")) {
          ImGui::DragFloat3("Position", &playerCoverObjectSettings_.position_.x, 0.1f);
          Vector2 uvScale = playerCoverObject_->GetUvScale();
          ImGui::DragFloat2("UV Scale", &uvScale.x, 0.1f);
          if (playerCoverObject_) {
              playerCoverObject_->SetTranslate(playerCoverObjectSettings_.position_);
              playerCoverObject_->SetUvScale(uvScale);
          }

          ImGui::TreePop();
      }
      if (ImGui::TreeNode("Target Cover Object")) {
        for (int i = 0; i < targetCovers_.size(); ++i) {
          std::string label = "Target Cover " + std::to_string(i);
          if (ImGui::TreeNode(label.c_str())) {
            ImGui::DragFloat3("Position", &targetCovers_[i].position_.x, 0.1f);
            Vector2 uvScale = targetCovers_[i].object_->GetUvScale();
            ImGui::DragFloat2("UV Scale", &uvScale.x, 0.1f);
            if (targetCovers_[i].object_) {
              targetCovers_[i].object_->SetTranslate(
                  targetCovers_[i].position_);
              targetCovers_[i].object_->SetUvScale(uvScale);
            }
            ImGui::TreePop();
          }
        }

          ImGui::TreePop();
      }
        ImGui::TreePop();
    }

    //スプライト
    if (ImGui::TreeNode("Sprites")) {
      if (ImGui::TreeNode("Title Text Sprite")) {
          if (ImGui::DragFloat2("Position", &titleTextSpriteSettings_.translate_.x, 1.0f)) {
              titleTextSprite_->SetTranslate(titleTextSpriteSettings_.translate_);
          }
          if (ImGui::DragFloat2("Scale", &titleTextSpriteSettings_.scale_.x, 1.0f)) {
              titleTextSprite_->SetScale(titleTextSpriteSettings_.scale_);
          }
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Control Sprite")) {
        if (ImGui::DragFloat2("Position", &controlSpriteSettings_.translate_.x, 1.0f)) {
            controlSprite_->SetTranslate(controlSpriteSettings_.translate_);
        }
        if (ImGui::DragFloat2("Scale", &controlSpriteSettings_.scale_.x, 1.0f)) {
            controlSprite_->SetScale(controlSpriteSettings_.scale_);
        }
        ImGui::TreePop();
      }
      ImGui::TreePop();
    }

    ImGui::End();
#endif
}

bool TitleScene::IntersectRayAABB(const Vector3 &rayOrigin, const Vector3 &rayDir, const AABB &aabb, float &outT)
{
    float tmin = 0.0f;
    float tmax = (std::numeric_limits<float>::max)();
    auto checkAxis = [&](float origin, float dir, float minVal, float maxVal)  {
        if (std::abs(dir) < 1e-6f) {
            return (origin >= minVal && origin <= maxVal);
        }
        float invD = 1.0f / dir;
        float t1 = (minVal - origin) * invD;
        float t2 = (maxVal - origin) * invD;
        if (t1 > t2) std::swap(t1, t2);
        tmin = (std::max)(tmin, t1);
        tmax = (std::min)(tmax, t2);
        return tmin <= tmax;
        };
    if (!checkAxis(rayOrigin.x, rayDir.x, aabb.min.x, aabb.max.x)) return false;
    if (!checkAxis(rayOrigin.y, rayDir.y, aabb.min.y, aabb.max.y)) return false;
    if (!checkAxis(rayOrigin.z, rayDir.z, aabb.min.z, aabb.max.z)) return false;
    outT = tmin;
    return true;
}

bool TitleScene::IsTargetHiddenByCover(int targetIdx)
{
    // 一番左の敵（インデックス0）の判定
    if (targetIdx == 0) {
        if (targetBoards_.empty() || targetCovers_.empty()) return false;
        float enemyX = targetBoards_[0].targetBoardObject_->GetTranslate().x;
        float coverX = targetCovers_[0].position_.x; // -7.5f
        // 縦長カバーの半幅(0.5m)を考慮し、中心のズレが 0.6m 以内なら「隠れている」と判定
        const float kCoverBlockWidth = 0.6f;
        if (std::abs(enemyX - coverX) < kCoverBlockWidth) {
            return true; // カバーの陰に隠れている
        }
    }
    return false; // 露出している
}

AABB TitleScene::MakeAABB(const Vector3 &center, const Vector3 &size)
{
    Vector3 half = Multiply(0.5f, size);
    return { Subtract(center, half), Add(center, half) };
}
