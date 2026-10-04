#include "GameOverScene.h"
#include "DX12Context.h"
#include "TextureManager.h"
#include "ImGuiManager.h"
#include "ParticleManager.h"
#include "Input.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "BlendMode.h"
#include "SceneManager.h"
#include "ApplicationConfig.h" // kDeltaTimeなど
// 計算用関数など
#include "MathUtils.h"
#include "MatrixGenerators.h"

// using
using namespace MathUtils;
using namespace MathGenerators;
using namespace BlendMode;

void GameOverScene::Initialize()
{
    // カメラ生成
    camera_ = std::make_unique<Camera>(); // メモリ確保と同時にスマートポインタ化
    camera_->Initialize();
    camera_->SetRotate({ 0.3f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 4.0f, -10.0f });

    gameCameraRotate_ = camera_->GetRotate();
    gameCameraTranslate_ = camera_->GetTranslate();

    debugCamera_.Initialize();

    // テクスチャ読み込み
    TextureManager::GetInstance()->LoadTexture(backGroundPath_);
    TextureManager::GetInstance()->LoadTexture(failedTextPath_);

    // --- 背景スプライト生成 ---
    backGroundSprite_ = std::make_unique<Sprite>();
    backGroundSprite_->Initialize(backGroundPath_);
    backGroundSprite_->SetAnchorPoint({ 0.0f, 0.0f });
    backGroundSprite_->SetTranslate({ 0.0f, 0.0f });
    backGroundSprite_->SetScale({ 1280.0f, 720.0f });

    // --- ゲームオーバー（失敗）テキストスプライト生成 ---
    failedTextSprite_ = std::make_unique<Sprite>();
    failedTextSprite_->Initialize(failedTextPath_);
    failedTextSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    failedTextSprite_->SetTranslate({ 640.0f, 150.0f });
}

void GameOverScene::Update()
{
    // 入力の更新
    Input::GetInstance()->Update();
    // 3. UI処理 (ImGuiの定義)
    UpdateImGui();

    // カメラの更新処理
    UpdateGameCamera();

    if (Input::GetInstance()->IsKeyTriggered(DIK_RETURN) || Input::GetInstance()->IsKeyTriggered(DIK_SPACE)) {
        SceneManager::GetInstance()->ChangeScene("TITLE");
    }

    // --- 背景スプライトの更新 ---
    if (backGroundSprite_) {
        backGroundSprite_->Update();
    }

    // --- ゲームオーバーテキストスプライトの更新 ---
    if (failedTextSprite_) {
        failedTextSprite_->Update();
    }
}

void GameOverScene::Draw()
{
    // スプライトの描画
    // 描画設定
    SpriteCommon::GetInstance()->SetCommonDrawSettings(
        static_cast<BlendState>(currentBlendMode_));

    if (isShowSprite_) {
        // 背景スプライトを最背面に描画
        if (backGroundSprite_) {
            backGroundSprite_->Draw();
        }

        // ゲームオーバーテキストを描画
        if (failedTextSprite_) {
            failedTextSprite_->Draw();
        }
    }
}

void GameOverScene::Finalize()
{
    // Object3dCommonの参照をクリア（次のシーン切り替え前に）
    Object3dCommon::GetInstance()->SetDefaultCamera(nullptr);

    backGroundSprite_.reset();
    failedTextSprite_.reset();
}

void GameOverScene::UpdateGameCamera()
{

    if (Input::GetInstance()->IsKeyTriggered(DIK_F1)) {
        useDebugCamera_ = !useDebugCamera_;
    }

    if (!useDebugCamera_) {
        // キー入力によるカメラ操作
        if (Input::GetInstance()->IsKeyDown(DIK_LEFT)) {
            gameCameraTranslate_.x -= 0.1f;
        }
        if (Input::GetInstance()->IsKeyDown(DIK_RIGHT)) {
            gameCameraTranslate_.x += 0.1f;
        }
        if (Input::GetInstance()->IsKeyDown(DIK_UP)) {
            gameCameraTranslate_.y += 0.1f;
        }
        if (Input::GetInstance()->IsKeyDown(DIK_DOWN)) {
            gameCameraTranslate_.y -= 0.1f;
        }
        // 共通のリセットキー
        if (Input::GetInstance()->IsKeyDown(DIK_R)) {
            gameCameraTranslate_ = { 0.0f, 2.0f, -15.0f };
        }

        camera_->SetTranslate(gameCameraTranslate_);
    }
    else {
        debugCamera_.Update();
        camera_->SetRotate(debugCamera_.GetRotation());
        camera_->SetTranslate(debugCamera_.GetTranslate());
    }
    camera_->Update();
}

void GameOverScene::UpdateImGui()
{
#ifdef USE_IMGUI
    // シーンの表示
    ImGui::Begin("Scene");
    ImGui::Text("Game Over Scene");
    ImGui::Text("Press ENTER or SPACE to return to Title");
    if (ImGui::Button("Return to Title")) {
        SceneManager::GetInstance()->ChangeScene("TITLE");
    }
    ImGui::SameLine();
    if (ImGui::Button("Retry Shooting")) {
        SceneManager::GetInstance()->ChangeScene("SHOOTING");
    }
    ImGui::End();

#endif
}