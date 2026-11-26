#pragma once
#include "MathTypes.h"
#include "Sprite.h"
#include <memory>

class ShootingHUD;
class ShootingPlayerController;

/// <summary>
/// シューティングシーンのゲームオーバー画面（敗北要因・RETRY/TITLE 選択・暗転遷移）を管理するクラス
/// </summary>
class ShootingGameOverOverlay {
public:
    enum class DeathReason {
        NormalBullet,
        EngineerRocket,
        CoverBlast,
        TimeUp,
        DroneExplosion
    };

    enum class SelectOption {
        None,
        Retry,
        Title
    };

    struct UpdateResult {
        bool hoverAny = false;
        bool updateCrosshair = false;
        bool requestRetry = false;
        bool requestTitle = false;
    };

    ShootingGameOverOverlay() = default;
    ~ShootingGameOverOverlay() = default;

    void Initialize();
    void Begin(DeathReason reason, float& outGameOverTimer);
    UpdateResult Update(float phaseTimer, float& gameOverTimer, const Vector2& mousePos, bool isClick,
                        ShootingHUD* hud, ShootingPlayerController* playerController);
    void Draw();

    const Vector2& GetRetryPos() const { return retryPos_; }

#ifdef USE_IMGUI
    void UpdateImGui();
#endif // USE_IMGUI

private:
    static bool IsPointInRect(const Vector2& point, const Vector2& center, const Vector2& size);

    static constexpr float kGameOverBlinkDuration = 0.6f;
    static constexpr float kGameOverFadeOutDuration = 0.8f;
    static constexpr float kGameOverWaitAfterFade = 0.5f;

    DeathReason deathReason_ = DeathReason::NormalBullet;
    SelectOption selectedOption_ = SelectOption::None;
    float transitionTimer_ = 0.0f;

    std::unique_ptr<Sprite> retrySprite_;
    std::unique_ptr<Sprite> titleSprite_;
    Vector2 retryPos_ = { 300.0f, 360.0f };
    Vector2 retryScale_ = { 300.0f, 150.0f };
    Vector2 titlePos_ = { 980.0f, 360.0f };
    Vector2 titleScale_ = { 300.0f, 150.0f };

    std::unique_ptr<Sprite> deathReasonSprite_;
    Vector2 deathReasonPos_ = { 640.0f, 140.0f };
    Vector2 deathReasonScale_ = { 512.0f, 64.0f };
};
