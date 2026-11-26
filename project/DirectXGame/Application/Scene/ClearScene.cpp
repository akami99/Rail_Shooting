#include "ClearScene.h"
#include "DX12Context.h"
#include "TextureManager.h"
#include "ImGuiManager.h"
#include "ParticleManager.h"
#include "Input.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "ModelManager.h"
#include "PostProcessManager.h"
#include "BlendMode.h"
#include "SceneManager.h"
#include "Win32Window.h"
#include "ApplicationConfig.h" // kDeltaTimeなど
// 計算用関数など
#include "MathUtils.h"
#include "MatrixGenerators.h"
#include <algorithm>
#include <limits>
#include <numbers>

// using
using namespace MathUtils;
using namespace MathGenerators;
using namespace BlendMode;

// 静的メンバ変数の実体
int ClearScene::clearScore_ = 0;
float ClearScene::clearRemainingTime_ = 0.0f;

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

void ClearScene::Initialize()
{
    // シューティングシーンの暗転状態を引き継いで、目を開けるフェードインから開始
    introFadeTimer_ = 0.0f;
    isTransitioningOut_ = false;
    transitionOutTimer_ = 0.0f;
    selectedAction_ = SelectAction::None;
    currentAimTarget_ = AimTarget::None;
    titleBoardState_ = BoardState::Standing;
    retryBoardState_ = BoardState::Standing;
    titleBoardFallAngle_ = 0.0f;
    retryBoardFallAngle_ = 0.0f;
    titleBoardCurrentYaw_ = titleBoardRot_.y;
    retryBoardCurrentYaw_ = retryBoardRot_.y;
    isCovering_ = false;
    coverYOffset_ = 0.0f;
    PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeVignette);
    PostProcessManager::GetInstance()->SetVignetteParams(0.0f, 5.0f);

    // カメラ生成（タイトルシーンと同様の視点）
    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    gameCameraRotate_ = { 0.0f, 0.0f, 0.0f };
    gameCameraTranslate_ = { 0.0f, 1.5f, -1.5f };
    camera_->SetRotate(gameCameraRotate_);
    camera_->SetTranslate(gameCameraTranslate_);
    camera_->SetFovY(45.0f * (std::numbers::pi_v<float> / 180.0f));

    debugCamera_.Initialize();

    Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());

    // モデル読み込み
    ModelManager::GetInstance()->LoadModel(floorModelDirectory_, floorModelPath_);
    ModelManager::GetInstance()->LoadModel(backWallModelDirectory_, backWallModelPath_);
    ModelManager::GetInstance()->LoadModel(playerCoverModelDirectory_, playerCoverModelPath_);
    ModelManager::GetInstance()->LoadModel(startBoardModelDirectory_, startBoardModelPath_);

    // テクスチャ読み込み
    TextureManager::GetInstance()->LoadTexture(tilePath_);
    TextureManager::GetInstance()->LoadTexture(clearTitleTexPath_);
    TextureManager::GetInstance()->LoadTexture(clearRetryTexPath_);
    TextureManager::GetInstance()->LoadTexture(whitePath_);
    TextureManager::GetInstance()->LoadTexture(clearTextPath_);
    TextureManager::GetInstance()->LoadTexture(timeTextPath_);
    TextureManager::GetInstance()->LoadTexture(scoreTextPath_);
    TextureManager::GetInstance()->LoadTexture(crosshairPath_);
    for (int i = 0; i < 10; ++i) {
        TextureManager::GetInstance()->LoadTexture("numbers/" + std::to_string(i) + ".png");
    }

    // 0. 3D背景・遮蔽オブジェクトの生成（タイトルシーンと同様）
    floorObject_ = std::make_unique<Object3d>();
    floorObject_->Initialize();
    floorObject_->SetModel(floorModelPath_);
    floorObject_->SetOverrideTexturePath(tilePath_);
    floorObject_->SetUvScale({ 10.0f, 10.0f });
    floorObject_->SetTranslate({ 0.0f, 0.0f, -15.0f });
    floorObject_->SetScale({ 4.0f, 1.0f, 3.0f });
    floorObject_->SetCamera(camera_.get());

    backWallObject_ = std::make_unique<Object3d>();
    backWallObject_->Initialize();
    backWallObject_->SetModel(backWallModelPath_);
    backWallObject_->SetOverrideTexturePath(tilePath_);
    backWallObject_->SetUvScale({ 10.0f, 4.0f });
    backWallObject_->SetTranslate({ 0.0f, 9.0f, 30.0f });
    backWallObject_->SetScale({ 4.0f, 3.0f, 1.0f });
    backWallObject_->SetCamera(camera_.get());

    playerCoverObject_ = std::make_unique<Object3d>();
    playerCoverObject_->Initialize();
    playerCoverObject_->SetModel(playerCoverModelPath_);
    playerCoverObject_->SetOverrideTexturePath(tilePath_);
    playerCoverObject_->SetUvScale({ 1.0f, 8.0f });
    playerCoverObject_->SetTranslate({ 0.0f, 0.5f, 0.5f });
    playerCoverObject_->SetCamera(camera_.get());

    // 左右看板オブジェクトの生成（タイトルシーンのstart看板を左右に配置し画像を貼り替え）
    titleBoardObject_ = std::make_unique<Object3d>();
    titleBoardObject_->Initialize();
    titleBoardObject_->SetModel(startBoardModelPath_);
    titleBoardObject_->SetOverrideTexturePath(clearTitleTexPath_);
    titleBoardObject_->SetCamera(camera_.get());

    retryBoardObject_ = std::make_unique<Object3d>();
    retryBoardObject_->Initialize();
    retryBoardObject_->SetModel(startBoardModelPath_);
    retryBoardObject_->SetOverrideTexturePath(clearRetryTexPath_);
    retryBoardObject_->SetCamera(camera_.get());

    // 1. クリアテキスト（画面上部中央）
    clearTextSprite_ = std::make_unique<Sprite>();
    clearTextSprite_->Initialize(clearTextPath_);
    clearTextSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    clearTextSprite_->SetTranslate({ 640.0f, 150.0f });
    clearTextSprite_->SetScale({ 420.0f, 105.0f });

    // 2. タイムテキスト & 経過時間数値スプライト（クリアテキスト中央の少し左下に配置）
    float elapsedTime = 60.0f - clearRemainingTime_;
    if (elapsedTime < 0.0f) elapsedTime = 0.0f;
    if (elapsedTime > 99.99f) elapsedTime = 99.99f;

    timeTextSprite_ = std::make_unique<Sprite>();
    timeTextSprite_->Initialize(timeTextPath_);
    timeTextSprite_->SetAnchorPoint({ 0.0f, 0.5f });
    timeTextSprite_->SetTranslate({ 360.0f, 330.0f });
    timeTextSprite_->SetScale({ 220.0f, 55.0f });

    int tempTime = static_cast<int>(std::round(elapsedTime * 100.0f));
    if (tempTime > 9999) tempTime = 9999;
    int tDigits[4] = { tempTime / 1000, (tempTime % 1000) / 100, (tempTime % 100) / 10, tempTime % 10 };

    timeDigits_.clear();
    for (int i = 0; i < 4; ++i) {
        auto unit = std::make_unique<Sprite>();
        unit->Initialize("numbers/" + std::to_string(tDigits[i]) + ".png");
        unit->SetAnchorPoint({ 0.0f, 0.5f });
        float x = 700.0f + i * 36.0f;
        if (i >= 2) {
            x += 12.0f; // ドット用の隙間
        }
        unit->SetTranslate({ x, 325.0f });
        unit->SetScale({ 28.0f, 44.0f });
        timeDigits_.push_back(std::move(unit));
    }

    timeDot_ = std::make_unique<Sprite>();
    timeDot_->Initialize(whitePath_);
    timeDot_->SetAnchorPoint({ 0.5f, 0.5f });
    timeDot_->SetTranslate({ 700.0f + 2 * 36.0f + 5.0f, 346.0f });
    timeDot_->SetScale({ 6.0f, 6.0f });

    // 3. スコアテキスト & スコア数値スプライト（タイムテキストの下に同じサイズで配置）
    scoreTextSprite_ = std::make_unique<Sprite>();
    scoreTextSprite_->Initialize(scoreTextPath_);
    scoreTextSprite_->SetAnchorPoint({ 0.0f, 0.5f });
    scoreTextSprite_->SetTranslate({ 360.0f, 460.0f });
    scoreTextSprite_->SetScale({ 220.0f, 55.0f });

    int tempScore = clearScore_;
    if (tempScore > 9999) tempScore = 9999;
    if (tempScore < 0) tempScore = 0;
    int sDigits[4] = { tempScore / 1000, (tempScore % 1000) / 100, (tempScore % 100) / 10, tempScore % 10 };

    scoreDigits_.clear();
    for (int i = 0; i < 4; ++i) {
        auto unit = std::make_unique<Sprite>();
        unit->Initialize("numbers/" + std::to_string(sDigits[i]) + ".png");
        unit->SetAnchorPoint({ 0.0f, 0.5f });
        scoreDigits_.push_back(std::move(unit));
    }

    // 4. 照準（クロスヘア）スプライト
    crosshairSprite_ = std::make_unique<Sprite>();
    crosshairSprite_->Initialize(crosshairPath_);
    crosshairSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    crosshairSprite_->SetScale({ 64.0f, 64.0f });
    crosshairSprite_->SetColor(kCrosshairColorNoAim);

    ApplyUILayout();
}

void ClearScene::Update()
{
    // 退場フェードアウト（目を閉じてシーン遷移）
    if (isTransitioningOut_) {
        transitionOutTimer_ += kDeltaTime;
        float t = std::clamp(transitionOutTimer_ / kTransitionOutDuration, 0.0f, 1.0f);
        float easedT = t * t;
        float scale = 16.0f * (1.0f - easedT);
        float exponent = 0.8f + (5.0f - 0.8f) * easedT;
        PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeVignette);
        PostProcessManager::GetInstance()->SetVignetteParams(scale, exponent);

        if (transitionOutTimer_ >= kTransitionOutDuration) {
            if (selectedAction_ == SelectAction::Retry) {
                SceneManager::GetInstance()->ChangeScene("SHOOTING");
            } else {
                SceneManager::GetInstance()->ChangeScene("TITLE");
            }
            return;
        }
    } else if (introFadeTimer_ < kIntroFadeDuration) {
        // 入場フェードイン（暗転から目を開ける）
        introFadeTimer_ += kDeltaTime;
        float t = std::clamp(introFadeTimer_ / kIntroFadeDuration, 0.0f, 1.0f);
        float eased = 1.0f - (1.0f - t) * (1.0f - t);
        float scale = 16.0f * eased;
        float exponent = 5.0f + (0.8f - 5.0f) * eased;
        if (t >= 1.0f) {
            PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeCopy);
        } else {
            PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeVignette);
            PostProcessManager::GetInstance()->SetVignetteParams(scale, exponent);
        }
    }

    // 入力の更新
    Input::GetInstance()->Update();
    // 3. UI処理 (ImGuiの定義)
    UpdateImGui();

    // カバー更新（SPACEキーでしゃがみ＆UI連動）
    if (!isTransitioningOut_) {
        UpdateCover();
    }

    // カメラの更新処理
    UpdateGameCamera();

    // 照準と左右看板のレイ判定・射撃・倒れアニメーション
    if (!isTransitioningOut_) {
        UpdateAimAndBoards();
    }

    // ENTERキーでタイトルへ（SPACEはカバー操作に使用）
    if (!isTransitioningOut_ && selectedAction_ == SelectAction::None &&
        Input::GetInstance()->IsKeyTriggered(DIK_RETURN)) {
        selectedAction_ = SelectAction::Title;
        isTransitioningOut_ = true;
        transitionOutTimer_ = 0.0f;
    }

    // 3Dオブジェクトの更新
    if (floorObject_) floorObject_->Update();
    if (backWallObject_) backWallObject_->Update();
    if (playerCoverObject_) playerCoverObject_->Update();
    if (titleBoardObject_) titleBoardObject_->Update();
    if (retryBoardObject_) retryBoardObject_->Update();

    // スプライトの更新
    if (clearTextSprite_) clearTextSprite_->Update();
    if (timeTextSprite_) timeTextSprite_->Update();
    if (scoreTextSprite_) scoreTextSprite_->Update();
    for (const auto& unit : timeDigits_) {
        if (unit) unit->Update();
    }
    if (timeDot_) timeDot_->Update();
    for (const auto& unit : scoreDigits_) {
        if (unit) unit->Update();
    }
}

void ClearScene::Draw()
{
    // 3Dオブジェクトの描画
    Object3dCommon::GetInstance()->SetCommonDrawSettings(
        static_cast<BlendState>(currentBlendMode_));

    if (floorObject_) floorObject_->Draw();
    if (backWallObject_) backWallObject_->Draw();
    if (playerCoverObject_) playerCoverObject_->Draw();
    if (titleBoardObject_) titleBoardObject_->Draw();
    if (retryBoardObject_) retryBoardObject_->Draw();

    // スプライトの描画
    SpriteCommon::GetInstance()->SetCommonDrawSettings(
        static_cast<BlendState>(currentBlendMode_));

    if (isShowSprite_) {
        if (clearTextSprite_) clearTextSprite_->Draw();
        if (timeTextSprite_) timeTextSprite_->Draw();
        if (scoreTextSprite_) scoreTextSprite_->Draw();
        for (const auto& unit : timeDigits_) {
            if (unit) unit->Draw();
        }
        if (timeDot_) timeDot_->Draw();
        for (const auto& unit : scoreDigits_) {
            if (unit) unit->Draw();
        }
        if (crosshairSprite_) crosshairSprite_->Draw();
    }
}

void ClearScene::Finalize()
{
    Object3dCommon::GetInstance()->SetDefaultCamera(nullptr);

    floorObject_.reset();
    backWallObject_.reset();
    playerCoverObject_.reset();
    titleBoardObject_.reset();
    retryBoardObject_.reset();

    clearTextSprite_.reset();
    timeTextSprite_.reset();
    scoreTextSprite_.reset();
    timeDigits_.clear();
    timeDot_.reset();
    scoreDigits_.clear();
    crosshairSprite_.reset();
}

void ClearScene::UpdateGameCamera()
{
    if (Input::GetInstance()->IsKeyTriggered(DIK_F1)) {
        useDebugCamera_ = !useDebugCamera_;
    }

    if (!useDebugCamera_) {
        Vector3 camPos = gameCameraTranslate_;
        camPos.y += coverYOffset_;
        camera_->SetRotate(gameCameraRotate_);
        camera_->SetTranslate(camPos);
    } else {
        debugCamera_.Update();
        camera_->SetRotate(debugCamera_.GetRotation());
        camera_->SetTranslate(debugCamera_.GetTranslate());
    }
    camera_->Update();
}

void ClearScene::ApplyUILayout()
{
    const float uiOffsetY = coverYOffset_ * 40.0f;

    if (clearTextSprite_) {
        clearTextSprite_->SetTranslate({ clearTextPos_.x, clearTextPos_.y + uiOffsetY });
        clearTextSprite_->SetScale(clearTextScale_);
    }

    if (timeTextSprite_) {
        timeTextSprite_->SetTranslate({ timeTextPos_.x, timeTextPos_.y + uiOffsetY });
        timeTextSprite_->SetScale(timeTextScale_);
    }

    for (size_t i = 0; i < timeDigits_.size(); ++i) {
        if (timeDigits_[i]) {
            float x = timeDigitsPos_.x + static_cast<float>(i) * timeDigitSpacing_;
            if (i >= 2) {
                x += timeDotGap_;
            }
            timeDigits_[i]->SetTranslate({ x, timeDigitsPos_.y + uiOffsetY });
            timeDigits_[i]->SetScale(timeDigitScale_);
        }
    }

    if (timeDot_) {
        timeDot_->SetTranslate({
            timeDigitsPos_.x + 2.0f * timeDigitSpacing_ + timeDotOffset_.x,
            timeDigitsPos_.y + timeDotOffset_.y + uiOffsetY
        });
        timeDot_->SetScale(timeDotScale_);
    }

    if (scoreTextSprite_) {
        scoreTextSprite_->SetTranslate({ scoreTextPos_.x, scoreTextPos_.y + uiOffsetY });
        scoreTextSprite_->SetScale(scoreTextScale_);
    }

    for (size_t i = 0; i < scoreDigits_.size(); ++i) {
        if (scoreDigits_[i]) {
            float x = scoreDigitsPos_.x + static_cast<float>(i) * scoreDigitSpacing_;
            scoreDigits_[i]->SetTranslate({ x, scoreDigitsPos_.y + uiOffsetY });
            scoreDigits_[i]->SetScale(scoreDigitScale_);
        }
    }

    if (titleBoardObject_) {
        titleBoardObject_->SetTranslate(titleBoardPos_);
        titleBoardObject_->SetRotation({ titleBoardRot_.x + titleBoardFallAngle_, titleBoardCurrentYaw_, titleBoardRot_.z });
        titleBoardObject_->SetScale(titleBoardScale_);
    }

    if (retryBoardObject_) {
        retryBoardObject_->SetTranslate(retryBoardPos_);
        retryBoardObject_->SetRotation({ retryBoardRot_.x + retryBoardFallAngle_, retryBoardCurrentYaw_, retryBoardRot_.z });
        retryBoardObject_->SetScale(retryBoardScale_);
    }
}

AABB ClearScene::MakeAABB(const Vector3& center, const Vector3& size)
{
    Vector3 half = Multiply(0.5f, size);
    return { Subtract(center, half), Add(center, half) };
}

bool ClearScene::IntersectRayAABB(const Vector3 &rayOrigin,
    const Vector3 &rayDir, const AABB &aabb,
    float &outT)
{
    float tmin = 0.0f;
    float tmax = (std::numeric_limits<float>::max)();
    auto checkAxis = [&](float origin, float dir, float minVal, float maxVal) {
        if (std::abs(dir) < 1e-6f) {
            return (origin >= minVal && origin <= maxVal);
        }
        float invD = 1.0f / dir;
        float t1 = (minVal - origin) * invD;
        float t2 = (maxVal - origin) * invD;
        if (t1 > t2) {
            std::swap(t1, t2);
        }
        tmin = (std::max)(tmin, t1);
        tmax = (std::min)(tmax, t2);
        return tmin <= tmax;
        };
    if (!checkAxis(rayOrigin.x, rayDir.x, aabb.min.x, aabb.max.x)) { return false; }
    if (!checkAxis(rayOrigin.y, rayDir.y, aabb.min.y, aabb.max.y)) { return false; }
    if (!checkAxis(rayOrigin.z, rayDir.z, aabb.min.z, aabb.max.z)) { return false; }
    outT = tmin;
    return true;
}

void ClearScene::UpdateCover()
{
    isCovering_ = Input::GetInstance()->IsKeyDown(DIK_SPACE);
    float targetOffset = isCovering_ ? kCoverYTarget : 0.0f;
    coverYOffset_ += (targetOffset - coverYOffset_) * 0.15f;
    ApplyUILayout();
}

void ClearScene::UpdateAimAndBoards()
{
    // 1. クロスヘアをマウス位置に追従
    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(Win32Window::GetInstance()->GetHwnd(), &mousePos);
    Vector2 mPos = { static_cast<float>(mousePos.x), static_cast<float>(mousePos.y) };

    if (crosshairSprite_) {
        crosshairSprite_->SetTranslate(mPos);
    }

    // 1.5 各看板の正面〜画面中央の範囲で照準のX位置に合わせて看板を向かせる
    const float screenCenterX = static_cast<float>(Win32Window::kClientWidth) * 0.5f;
    if (titleBoardState_ == BoardState::Standing) {
        float titleScreenX = WorldToScreen(titleBoardPos_, camera_.get()).x;
        float denom = (std::max)(1.0f, screenCenterX - titleScreenX);
        float tTitle = std::clamp((mPos.x - titleScreenX) / denom, 0.0f, 1.0f);
        float targetYaw = titleBoardRot_.y * tTitle; // 0.0(正面) <-> titleBoardRot_.y(中央向き)
        titleBoardCurrentYaw_ += (targetYaw - titleBoardCurrentYaw_) * 0.15f;
        if (titleBoardObject_) {
            titleBoardObject_->SetRotation({ titleBoardRot_.x + titleBoardFallAngle_, titleBoardCurrentYaw_, titleBoardRot_.z });
        }
    }
    if (retryBoardState_ == BoardState::Standing) {
        float retryScreenX = WorldToScreen(retryBoardPos_, camera_.get()).x;
        float denom = (std::max)(1.0f, retryScreenX - screenCenterX);
        float tRetry = std::clamp((retryScreenX - mPos.x) / denom, 0.0f, 1.0f);
        float targetYaw = retryBoardRot_.y * tRetry; // 0.0(正面) <-> retryBoardRot_.y(中央向き)
        retryBoardCurrentYaw_ += (targetYaw - retryBoardCurrentYaw_) * 0.15f;
        if (retryBoardObject_) {
            retryBoardObject_->SetRotation({ retryBoardRot_.x + retryBoardFallAngle_, retryBoardCurrentYaw_, retryBoardRot_.z });
        }
    }

    // 2. マウス座標から3D空間のレイを算出
    float x = mPos.x / static_cast<float>(Win32Window::kClientWidth) * 2.0f - 1.0f;
    float y = 1.0f - mPos.y / static_cast<float>(Win32Window::kClientHeight) * 2.0f;
    Matrix4x4 invVP = Inverse(camera_->GetViewProjectionMatrix());
    Vector3 nearPos = TransformPoint({ x, y, 0.0f }, invVP);
    Vector3 farPos = TransformPoint({ x, y, 1.0f }, invVP);
    Vector3 rayDir = Normalize(Subtract(farPos, nearPos));

    // 3. 左右看板との交差判定（カバー中は狙えない）
    currentAimTarget_ = AimTarget::None;
    if (!isCovering_ && selectedAction_ == SelectAction::None) {
        float closestT = (std::numeric_limits<float>::max)();
        AimTarget closestHit = AimTarget::None;
        float t = 0.0f;

        auto makeBoardAABB = [this](const Vector3& pos, const Vector3& rot, const Vector3& scale) {
            float w = kBoardBaseAabbSize.x * scale.x;
            float h = kBoardBaseAabbSize.y * scale.y;
            float d = kBoardBaseAabbSize.z * scale.z;
            float cosY = std::abs(std::cos(rot.y));
            float sinY = std::abs(std::sin(rot.y));
            Vector3 rotatedSize = {
                w * cosY + d * sinY,
                h,
                w * sinY + d * cosY
            };
            Vector3 center = pos;
            center.y += h * 0.5f + 0.75f * scale.y;
            return MakeAABB(center, rotatedSize);
        };

        if (titleBoardObject_ && titleBoardState_ == BoardState::Standing) {
            Vector3 currentRot = { titleBoardRot_.x, titleBoardCurrentYaw_, titleBoardRot_.z };
            AABB box = makeBoardAABB(titleBoardPos_, currentRot, titleBoardScale_);
            if (IntersectRayAABB(nearPos, rayDir, box, t) && t < closestT) {
                closestT = t;
                closestHit = AimTarget::TitleBoard;
            }
        }

        if (retryBoardObject_ && retryBoardState_ == BoardState::Standing) {
            Vector3 currentRot = { retryBoardRot_.x, retryBoardCurrentYaw_, retryBoardRot_.z };
            AABB box = makeBoardAABB(retryBoardPos_, currentRot, retryBoardScale_);
            if (IntersectRayAABB(nearPos, rayDir, box, t) && t < closestT) {
                closestT = t;
                closestHit = AimTarget::RetryBoard;
            }
        }

        currentAimTarget_ = closestHit;
    }

    // 4. クロスヘアの色更新
    if (crosshairSprite_) {
        if (currentAimTarget_ != AimTarget::None) {
            crosshairSprite_->SetColor(kCrosshairColorAim);
        } else {
            crosshairSprite_->SetColor(kCrosshairColorNoAim);
        }
        crosshairSprite_->Update();
    }

    // 5. 左クリックによる看板射撃（カバー中は撃てない）
    if (!isCovering_ && selectedAction_ == SelectAction::None && Input::GetInstance()->IsMouseButtonTriggered(0)) {
        if (currentAimTarget_ == AimTarget::TitleBoard) {
            titleBoardState_ = BoardState::Falling;
            selectedAction_ = SelectAction::Title;
        } else if (currentAimTarget_ == AimTarget::RetryBoard) {
            retryBoardState_ = BoardState::Falling;
            selectedAction_ = SelectAction::Retry;
        }
    }

    // 6. 看板の倒れアニメーション更新（90度倒れたら暗転遷移を開始）
    const float maxAngle = std::numbers::pi_v<float> * 0.5f;
    if (titleBoardState_ == BoardState::Falling) {
        titleBoardFallAngle_ += kBoardFallSpeed * kDeltaTime;
        if (titleBoardFallAngle_ >= maxAngle) {
            titleBoardFallAngle_ = maxAngle;
            titleBoardState_ = BoardState::Down;
            isTransitioningOut_ = true;
            transitionOutTimer_ = 0.0f;
        }
        if (titleBoardObject_) {
            titleBoardObject_->SetRotation({ titleBoardRot_.x + titleBoardFallAngle_, titleBoardCurrentYaw_, titleBoardRot_.z });
        }
    }

    if (retryBoardState_ == BoardState::Falling) {
        retryBoardFallAngle_ += kBoardFallSpeed * kDeltaTime;
        if (retryBoardFallAngle_ >= maxAngle) {
            retryBoardFallAngle_ = maxAngle;
            retryBoardState_ = BoardState::Down;
            isTransitioningOut_ = true;
            transitionOutTimer_ = 0.0f;
        }
        if (retryBoardObject_) {
            retryBoardObject_->SetRotation({ retryBoardRot_.x + retryBoardFallAngle_, retryBoardCurrentYaw_, retryBoardRot_.z });
        }
    }
}

void ClearScene::UpdateImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin("Scene");
    ImGui::Text("Clear Scene");
    float elapsedTime = 60.0f - clearRemainingTime_;
    if (elapsedTime < 0.0f) elapsedTime = 0.0f;
    ImGui::Text("Clear Time : %.2f s", elapsedTime);
    ImGui::Text("Clear Score: %d", clearScore_);
    ImGui::Separator();
    ImGui::Text("Press ENTER to return to Title (SPACE: Cover)");
    if (ImGui::Button("Return to Title")) {
        SceneManager::GetInstance()->ChangeScene("TITLE");
    }
    ImGui::SameLine();
    if (ImGui::Button("Retry Shooting")) {
        SceneManager::GetInstance()->ChangeScene("SHOOTING");
    }

    ImGui::Separator();
    if (ImGui::TreeNode("UI Layout")) {
        bool layoutChanged = false;
        ImGui::Checkbox("Show Sprites", &isShowSprite_);

        if (ImGui::TreeNode("Clear Text")) {
            layoutChanged |= ImGui::DragFloat2("Pos##Clear", &clearTextPos_.x, 1.0f);
            layoutChanged |= ImGui::DragFloat2("Scale##Clear", &clearTextScale_.x, 1.0f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Time Text")) {
            layoutChanged |= ImGui::DragFloat2("Pos##TimeText", &timeTextPos_.x, 1.0f);
            layoutChanged |= ImGui::DragFloat2("Scale##TimeText", &timeTextScale_.x, 1.0f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Time Digits")) {
            layoutChanged |= ImGui::DragFloat2("Pos##TimeDigits", &timeDigitsPos_.x, 1.0f);
            layoutChanged |= ImGui::DragFloat2("Scale##TimeDigits", &timeDigitScale_.x, 1.0f);
            layoutChanged |= ImGui::DragFloat("Spacing##TimeDigits", &timeDigitSpacing_, 0.5f);
            layoutChanged |= ImGui::DragFloat("Dot Gap##TimeDigits", &timeDotGap_, 0.5f);
            layoutChanged |= ImGui::DragFloat2("Dot Offset##TimeDigits", &timeDotOffset_.x, 0.5f);
            layoutChanged |= ImGui::DragFloat2("Dot Scale##TimeDigits", &timeDotScale_.x, 0.5f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Score Text")) {
            layoutChanged |= ImGui::DragFloat2("Pos##ScoreText", &scoreTextPos_.x, 1.0f);
            layoutChanged |= ImGui::DragFloat2("Scale##ScoreText", &scoreTextScale_.x, 1.0f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Score Digits")) {
            layoutChanged |= ImGui::DragFloat2("Pos##ScoreDigits", &scoreDigitsPos_.x, 1.0f);
            layoutChanged |= ImGui::DragFloat2("Scale##ScoreDigits", &scoreDigitScale_.x, 1.0f);
            layoutChanged |= ImGui::DragFloat("Spacing##ScoreDigits", &scoreDigitSpacing_, 0.5f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Title Board (Left)")) {
            layoutChanged |= ImGui::DragFloat3("Pos##TitleBoard", &titleBoardPos_.x, 0.1f);
            layoutChanged |= ImGui::DragFloat3("Rot##TitleBoard", &titleBoardRot_.x, 0.01f);
            layoutChanged |= ImGui::DragFloat3("Scale##TitleBoard", &titleBoardScale_.x, 0.05f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Retry Board (Right)")) {
            layoutChanged |= ImGui::DragFloat3("Pos##RetryBoard", &retryBoardPos_.x, 0.1f);
            layoutChanged |= ImGui::DragFloat3("Rot##RetryBoard", &retryBoardRot_.x, 0.01f);
            layoutChanged |= ImGui::DragFloat3("Scale##RetryBoard", &retryBoardScale_.x, 0.05f);
            ImGui::TreePop();
        }

        if (layoutChanged) {
            ApplyUILayout();
        }
        ImGui::TreePop();
    }
    ImGui::End();
#endif
}