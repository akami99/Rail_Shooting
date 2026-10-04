#pragma once
#include "BaseScene.h"
#include <vector>
#include <memory>
#include "MathTypes.h"

// 必要なクラスをインクルード or 前方宣言
#include "DebugCamera.h"
#include "Camera.h"
#include "Sprite.h"

class GameOverScene : public BaseScene {
public:
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    // ゲームカメラの更新
    void UpdateGameCamera();

    // ImGui操作の更新
    void UpdateImGui();

private:
    // --- ここから下は MyGame.h から移動してきたゲーム固有変数 ---

    // カメラ(必ずスマートポインタにすること)
    std::unique_ptr<Camera> camera_;

    DebugCamera debugCamera_;
    bool useDebugCamera_ = false;
    Vector3 gameCameraRotate_{};
    Vector3 gameCameraTranslate_{};

    // ゲームオブジェクト(必ずスマートポインタにすること)
    std::unique_ptr<Sprite> backGroundSprite_;
    std::unique_ptr<Sprite> failedTextSprite_;

    // パーティクル設定など
    int currentBlendMode_ = 1;  // NormalBlend
    bool isShowSprite_ = true;

    // パス定数
    // テクスチャファイルパスを保持
    const std::string backGroundPath_ = "floor.png";
    const std::string failedTextPath_ = "gameOver/failedText.png";
};