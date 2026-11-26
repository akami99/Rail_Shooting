#pragma once
#include "BaseScene.h"
#include <vector>
#include <memory>
#include "MathTypes.h"

// 必要なクラスをインクルード or 前方宣言
#include "DebugCamera.h"
#include "Camera.h"
#include "Object3d.h"
#include "Sprite.h"

class ClearScene : public BaseScene {
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

    // UIレイアウトの反映
    void ApplyUILayout();

    // 照準と左右看板のレイ判定・射撃・倒れアニメーション更新
    void UpdateAimAndBoards();

    // カバー処理（SPACEキーでしゃがみ＆UI連動）
    void UpdateCover();

    // AABB生成ヘルパー
    static AABB MakeAABB(const Vector3& center, const Vector3& size);

    // 直方体とレイの交差判定
    bool IntersectRayAABB(const Vector3 &rayOrigin, const Vector3 &rayDir, const AABB &aabb, float &outT);

private:
    // カメラ
    std::unique_ptr<Camera> camera_;
    DebugCamera debugCamera_;
    bool useDebugCamera_ = false;
    Vector3 gameCameraRotate_{ 0.0f, 0.0f, 0.0f };
    Vector3 gameCameraTranslate_{ 0.0f, 1.5f, -1.5f };

    // 3Dオブジェクト（タイトルシーンと同様の背景・遮蔽オブジェクト・左右看板）
    std::unique_ptr<Object3d> floorObject_;
    std::unique_ptr<Object3d> backWallObject_;
    std::unique_ptr<Object3d> playerCoverObject_;
    std::unique_ptr<Object3d> titleBoardObject_;
    std::unique_ptr<Object3d> retryBoardObject_;

    // 入場フェードイン用（暗転状態から目を開ける）
    float introFadeTimer_ = 0.0f;
    static constexpr float kIntroFadeDuration = 0.6f;

    // UIスプライト
    std::unique_ptr<Sprite> clearTextSprite_;
    std::unique_ptr<Sprite> timeTextSprite_;
    std::unique_ptr<Sprite> scoreTextSprite_;
    std::unique_ptr<Sprite> crosshairSprite_;

    // タイム用数字スプライト（4桁 + ドット）
    std::vector<std::unique_ptr<Sprite>> timeDigits_;
    std::unique_ptr<Sprite> timeDot_;

    // スコア用数字スプライト（4桁）
    std::vector<std::unique_ptr<Sprite>> scoreDigits_;

    // 設定
    int currentBlendMode_ = 1;  // NormalBlend
    bool isShowSprite_ = true;

    // UIレイアウト調整用パラメータ
    Vector2 clearTextPos_ = { 640.0f, 86.0f };
    Vector2 clearTextScale_ = { 420.0f, 105.0f };

    Vector2 timeTextPos_ = { 420.0f, 220.0f };
    Vector2 timeTextScale_ = { 220.0f, 55.0f };
    Vector2 timeDigitsPos_ = { 672.0f, 215.0f };
    Vector2 timeDigitScale_ = { 40.0f, 40.0f };
    float timeDigitSpacing_ = 36.0f;
    float timeDotGap_ = 12.0f;
    Vector2 timeDotOffset_ = { 5.0f, 21.0f };
    Vector2 timeDotScale_ = { 6.0f, 6.0f };

    Vector2 scoreTextPos_ = { 417.0f, 340.0f };
    Vector2 scoreTextScale_ = { 220.0f, 55.0f };
    Vector2 scoreDigitsPos_ = { 672.0f, 335.0f };
    Vector2 scoreDigitScale_ = { 40.0f, 40.0f };
    float scoreDigitSpacing_ = 36.0f;

    // 看板配置調整用パラメータ
    Vector3 titleBoardPos_ = { -6.0f, 0.0f, 12.0f };
    Vector3 titleBoardRot_ = { 0.0f, -0.58f, 0.0f };
    Vector3 titleBoardScale_ = { 1.0f, 1.0f, 1.0f };
    Vector3 retryBoardPos_ = { 6.0f, 0.0f, 12.0f };
    Vector3 retryBoardRot_ = { 0.0f, 0.58f, 0.0f };
    Vector3 retryBoardScale_ = { 1.0f, 1.0f, 1.0f };

    // 照準・看板射撃選択用
    enum class BoardState {
        Standing,
        Falling,
        Down
    };
    enum class AimTarget {
        None,
        TitleBoard,
        RetryBoard
    };
    enum class SelectAction {
        None,
        Title,
        Retry
    };
    AimTarget currentAimTarget_ = AimTarget::None;
    SelectAction selectedAction_ = SelectAction::None;
    BoardState titleBoardState_ = BoardState::Standing;
    BoardState retryBoardState_ = BoardState::Standing;
    float titleBoardFallAngle_ = 0.0f;
    float retryBoardFallAngle_ = 0.0f;
    float titleBoardCurrentYaw_ = -0.58f;
    float retryBoardCurrentYaw_ = 0.58f;
    static constexpr float kBoardFallSpeed = 4.0f;
    static constexpr Vector3 kBoardBaseAabbSize = { 3.0f, 1.5f, 0.3f };
    static constexpr Vector4 kCrosshairColorAim = { 1.0f, 0.0f, 0.0f, 1.0f };
    static constexpr Vector4 kCrosshairColorNoAim = { 0.0f, 1.0f, 0.0f, 0.4f };

    // 退場フェードアウト用（目を閉じて遷移）
    bool isTransitioningOut_ = false;
    float transitionOutTimer_ = 0.0f;
    static constexpr float kTransitionOutDuration = 0.5f;

    // カバー管理（タイトルシーンと同設定）
    bool isCovering_ = false;
    float coverYOffset_ = 0.0f;
    static constexpr float kCoverYTarget = -0.5f;

    // パス定数
    const std::string floorModelDirectory_ = "Resources/Assets/Models/TitleScene/titleFloor";
    const std::string floorModelPath_ = "titleFloor.obj";
    const std::string backWallModelDirectory_ = "Resources/Assets/Models/TitleScene/backWall";
    const std::string backWallModelPath_ = "backWall.obj";
    const std::string playerCoverModelDirectory_ = "Resources/Assets/Models/TitleScene/playerCover";
    const std::string playerCoverModelPath_ = "playerCover.obj";
    const std::string startBoardModelDirectory_ = "Resources/Assets/Models/TitleScene/start";
    const std::string startBoardModelPath_ = "start.obj";
    const std::string tilePath_ = "tile.png";
    const std::string clearTitleTexPath_ = "clear/clearTitle.png";
    const std::string clearRetryTexPath_ = "clear/clearRetry.png";
    const std::string whitePath_ = "white.png";
    const std::string clearTextPath_ = "ui/clearText.png";
    const std::string timeTextPath_ = "ui/timeText.png";
    const std::string scoreTextPath_ = "ui/scoreText.png";
    const std::string crosshairPath_ = "ui/crosshair.png";
};