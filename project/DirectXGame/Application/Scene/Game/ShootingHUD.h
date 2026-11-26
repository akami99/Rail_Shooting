#pragma once
#include "MathTypes.h"
#include "Sprite.h"
#include <memory>
#include <vector>

/// <summary>
/// シューティングシーンのHUD（ライフ・スコア・タイマー・プログレスバー・案内UI・Ready/Go演出・カバー演出）を管理するクラス
/// </summary>
class ShootingHUD {
public:
    ShootingHUD() = default;
    ~ShootingHUD() = default;

    static constexpr float kReadyGoTotalDuration = 2.4f;

    void Initialize(const std::vector<float>& sectionProgresses, float maxProgress, int lifeSegments, float initialGameTimer);

    // 各状態での更新
    void UpdateReadyGo(float phaseTimer);
    void HideReadyGo();

    void UpdateLifeGauge(int playerHp, int maxPlayerHp, float lifeBlinkTimer);
    void ResetLifeGauge(int playerHp, int maxPlayerHp);

    void UpdateScoreAndGuides(int score, bool isCovering);
    void UpdateProgressBar(const Vector2& basePos, const Vector2& baseSize,
                           float progress, float maxProgress,
                           const std::vector<float>& sectionProgresses,
                           const std::vector<bool>& sectionCleared,
                           bool isSwarmBattle, bool isMovementPaused, float lifeBlinkTimer);
    void UpdateGameplayProgressBar(float progress, float maxProgress,
                                   const std::vector<float>& sectionProgresses,
                                   const std::vector<bool>& sectionCleared,
                                   bool isSwarmBattle, bool isMovementPaused, float lifeBlinkTimer);
    void UpdateGameOverProgressBar(float progress, float maxProgress,
                                   const std::vector<float>& sectionProgresses,
                                   const std::vector<bool>& sectionCleared,
                                   float lifeBlinkTimer);

    void UpdateGameplayTimer(float gameTimer);
    void UpdateGameOverTimer(float gameOverTimer);
    void ApplyGameOverFade(float remainAlpha);
    void ResetGameOverColors();

    // 描画
    void DrawCover(bool isCovering);
    void DrawGameplayBackgrounds();
    void DrawGameOverBackgrounds();
    void DrawLifeGauge(int playerHp, int maxPlayerHp);
    void DrawProgressBar();
    void DrawScoreAndGuides();
    void DrawTimer(bool showDecimals);
    void DrawReadyGo();

#ifdef USE_IMGUI
    void UpdateGuidesImGui();
    bool UpdateReadyGoImGui();
    void UpdateProgressBarImGui();
    void UpdateTimerImGui(float& gameOverTimer);
#endif

private:
    void UpdateTimerInternal(float timeValue, const Vector2& basePos, const Vector2& bgSize,
                             const Vector2& digitSize, float spacing, const Vector2& dotSize, bool showDecimals);

private:
    int lifeSegments_ = 3;

    // ライフUI
    std::unique_ptr<Sprite> lifeBg_;
    std::vector<std::unique_ptr<Sprite>> lifeUnits_;
    std::vector<std::unique_ptr<Sprite>> lifeDividers_;

    // プログレスバーUI
    std::unique_ptr<Sprite> progressBg_;
    std::unique_ptr<Sprite> progressBar_;
    std::vector<std::unique_ptr<Sprite>> waveTickSprites_;
    std::vector<std::unique_ptr<Sprite>> waveIconSprites_;
    std::unique_ptr<Sprite> bossWaveIconSprite_;
    std::unique_ptr<Sprite> playerMarkerSprite_;

    Vector2 progressPos_ = { 480.0f, 48.0f };
    Vector2 progressSize_ = { 300.0f, 16.0f };
    float bossIconScale_ = 24.0f;
    float bossIconSpacing_ = 6.0f;
    Vector2 gameOverProgressPos_ = { 440.0f, 620.0f };
    Vector2 gameOverProgressSize_ = { 400.0f, 18.0f };

    // スコア・タイマーUI
    std::unique_ptr<Sprite> scoreBg_;
    std::vector<std::unique_ptr<Sprite>> scoreUnits_;
    std::unique_ptr<Sprite> timerBg_;
    std::vector<std::unique_ptr<Sprite>> timerUnits_;
    std::unique_ptr<Sprite> timerDot_;

    Vector2 timerPos_ = { 550.0f, 70.0f };
    Vector2 timerBgSize_ = { 160.0f, 42.0f };
    Vector2 timerDigitSize_ = { 20.0f, 32.0f };
    float timerDigitSpacing_ = 30.0f;
    Vector2 timerDotSize_ = { 4.0f, 4.0f };

    Vector2 gameOverTimerPos_ = { 540.0f, 310.0f };
    Vector2 gameOverTimerBgSize_ = { 200.0f, 100.0f };
    Vector2 gameOverTimerDigitSize_ = { 52.0f, 76.0f };
    float gameOverTimerDigitSpacing_ = 60.0f;
    Vector2 gameOverTimerDotSize_ = { 6.0f, 6.0f };

    // ESC / R / 操作説明スプライト
    std::unique_ptr<Sprite> escTextSprite_;
    Vector2 escTextPos_ = { 30.0f, 85.0f };
    Vector2 escTextScale_ = { 160.0f, 40.0f };

    std::unique_ptr<Sprite> rTextSprite_;
    Vector2 rTextPos_ = { 30.0f, 130.0f };
    Vector2 rTextScale_ = { 160.0f, 40.0f };

    std::unique_ptr<Sprite> controlSprite_;
    Vector2 controlPos_ = { 1120.0f, 620.0f };
    Vector2 controlScale_ = { 256.0f, 128.0f };

    // Ready / Go 帯リール演出
    std::unique_ptr<Sprite> readyGoRibbon_;
    std::unique_ptr<Sprite> readyTextSprite_;
    std::unique_ptr<Sprite> goTextSprite_;
    Vector2 readyGoRibbonSize_ = { 1280.0f, 130.0f };
    Vector2 readyTextScale_ = { 320.0f, 160.0f };
    Vector2 goTextScale_ = { 320.0f, 160.0f };
    float readyGoRibbonY_ = 360.0f;
    bool isRibbonVisible_ = false;
    bool isReadyVisible_ = false;
    bool isGoVisible_ = false;

    // カバー演出UI
    std::unique_ptr<Sprite> coverOverlay_;
};
