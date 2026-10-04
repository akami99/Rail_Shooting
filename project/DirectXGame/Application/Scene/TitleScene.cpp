#include "TitleScene.h"
#include "ApplicationConfig.h" // kDeltaTimeなど
#include "BlendMode.h"
#include "DX12Context.h"
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
    // カメラ生成
    camera_ = std::make_unique<Camera>(); // メモリ確保と同時にスマートポインタ化
    camera_->Initialize();
    camera_->SetTranslate(cameraSettings_.translate);
    camera_->SetRotate(cameraSettings_.rotate);

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

    // オブジェクトの初期化
    // --- フロアオブジェクト生成 ---
    floorObject_ = std::make_unique<Object3d>();
    floorObject_->Initialize();
    floorObject_->SetModel(floorModelPath_);
    floorObject_->SetTranslate(floorObjectPosition_);
    floorObject_->SetCamera(camera_.get());

    // --- バックウォールオブジェクト生成 ---
    backWallObject_ = std::make_unique<Object3d>();
    backWallObject_->Initialize();
    backWallObject_->SetModel(backWallModelPath_);
    backWallObject_->SetTranslate(backWallObjectSettings_.position_);
    backWallObject_->SetRotation(backWallObjectSettings_.rotation_);
    backWallObject_->SetCamera(camera_.get());

    // --- プレイヤーカバーオブジェクト生成 ---
    playerCoverObject_ = std::make_unique<Object3d>();
    playerCoverObject_->Initialize();
    playerCoverObject_->SetModel(playerCoverModelPath_);
    playerCoverObject_->SetTranslate(playerCoverObjectPosition_);
    playerCoverObject_->SetCamera(camera_.get());

    // --- ターゲットカバーオブジェクト生成 ---
    targetCoverObject_ = std::make_unique<Object3d>();
    targetCoverObject_->Initialize();
    targetCoverObject_->SetModel(targetCoverModelPath_);
    targetCoverObject_->SetTranslate(targetCoverObjectPosition_);
    targetCoverObject_->SetCamera(camera_.get());

    // --- スタートボードオブジェクト生成 ---
    startBoardObject_ = std::make_unique<Object3d>();
    startBoardObject_->Initialize();
    startBoardObject_->SetModel(startBoardModelPath_);
    startBoardObject_->SetTranslate(startBoardObjectSettings_.position_);
    startBoardObject_->SetRotation(startBoardObjectSettings_.rotation_);
    startBoardObject_->SetCamera(camera_.get());

    // --- ターゲットボードオブジェクト生成 ---
    for (int i = 0; i < 2; ++i) {
        TargetBoardInfo targetBoardInfo;
        targetBoardInfo.targetBoardObject_ = std::make_unique<Object3d>();
        targetBoardInfo.targetBoardObject_->Initialize();
        targetBoardInfo.targetBoardObject_->SetModel(targetBoardModelPath_);

        if (i == 1) {
            Vector3 targetBoardPosition = targetBoardBasePosition_;
            targetBoardPosition.x = -targetBoardPosition.x;
            targetBoardInfo.targetBoardObject_->SetTranslate(targetBoardPosition);
        } else {
            targetBoardInfo.targetBoardObject_->SetTranslate(targetBoardBasePosition_);
        }
        targetBoardInfo.targetBoardObject_->SetCamera(camera_.get());
        targetBoards_.push_back(std::move(targetBoardInfo));
    }

    // スプライトの初期化
    // --- タイトルテキストスプライト生成 ---
    titleTextSprite_ = std::make_unique<Sprite>();
    titleTextSprite_->Initialize(titleTextPath_);
    titleTextSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    titleTextSprite_->SetTranslate({ 640.0f, 180.0f });
}

void TitleScene::Update()
{
    // 入力の更新
    Input::GetInstance()->Update();
    // 3. UI処理 (ImGuiの定義)
    UpdateImGui();

    // カメラの更新処理
    UpdateGameCamera();

    if (Input::GetInstance()->IsKeyTriggered(DIK_RETURN) ||
        Input::GetInstance()->IsKeyTriggered(DIK_SPACE)) {
        SceneManager::GetInstance()->ChangeScene("SHOOTING");
    }

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
    if (targetCoverObject_) {
        targetCoverObject_->Update();
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
    if (targetCoverObject_) {
      targetCoverObject_->Draw();
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

    // スプライトの描画
    // 描画設定
    SpriteCommon::GetInstance()->SetCommonDrawSettings(static_cast<BlendState>(currentBlendMode_));

    // タイトルテキストを描画
    if (titleTextSprite_) {
        titleTextSprite_->Draw();
    }
}

void TitleScene::Finalize()
{
    // Object3dCommonの参照をクリア（次のシーン切り替え前に）
    Object3dCommon::GetInstance()->SetDefaultCamera(nullptr);

    floorObject_.reset();
    backWallObject_.reset();
    playerCoverObject_.reset();
    targetCoverObject_.reset();
    startBoardObject_.reset();
    targetBoards_.clear();
    titleTextSprite_.reset();
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
    ImGui::Begin("Scene");
    ImGui::Text("Title Scene");
    ImGui::Text("Press ENTER or SPACE to start");
    if (ImGui::Button("Start Shooting")) {
        SceneManager::GetInstance()->ChangeScene("SHOOTING");
    }
    ImGui::End();
#endif
}