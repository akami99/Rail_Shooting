#include "ShootingHUD.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

void ShootingHUD::Initialize(const std::vector<float>& sectionProgresses, float maxProgress, int lifeSegments, float initialGameTimer) {
  lifeSegments_ = lifeSegments;

  // 1. ライフUI
  lifeBg_ = std::make_unique<Sprite>();
  lifeBg_->Initialize("white.png");
  lifeBg_->SetTranslate({ 1140.0f, 30.0f });
  lifeBg_->SetScale({ 110.0f, 50.0f });
  lifeBg_->SetColor({ 0.2f, 0.2f, 0.2f, 0.8f });

  lifeUnits_.clear();
  for (int i = 0; i < lifeSegments_; ++i) {
    auto unit = std::make_unique<Sprite>();
    unit->Initialize("white.png");
    unit->SetTranslate({ 1150.0f + i * 30.0f, 37.0f });
    unit->SetScale({ 30.0f, 36.0f });
    unit->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
    lifeUnits_.push_back(std::move(unit));
  }

  lifeDividers_.clear();
  for (int i = 0; i < lifeSegments_ - 1; ++i) {
    auto divider = std::make_unique<Sprite>();
    divider->Initialize("white.png");
    divider->SetTranslate({ 1150.0f + (i + 1) * 30.0f - 1.0f, 37.0f });
    divider->SetScale({ 2.0f, 36.0f });
    divider->SetColor({ 0.15f, 0.15f, 0.15f, 0.95f });
    lifeDividers_.push_back(std::move(divider));
  }

  // 2. プログレスバーUI
  TextureManager::GetInstance()->LoadTexture("ui/waveIconNormal.png");
  TextureManager::GetInstance()->LoadTexture("ui/waveIconBoss.png");
  TextureManager::GetInstance()->LoadTexture("ui/playerMarker.png");

  progressBg_ = std::make_unique<Sprite>();
  progressBg_->Initialize("white.png");
  progressBg_->SetTranslate(progressPos_);
  progressBg_->SetScale(progressSize_);
  progressBg_->SetColor({ 0.1f, 0.15f, 0.35f, 0.75f });

  progressBar_ = std::make_unique<Sprite>();
  progressBar_->Initialize("white.png");
  progressBar_->SetTranslate(progressPos_);
  progressBar_->SetScale({ 0.0f, 16.0f });
  progressBar_->SetColor({ 0.2f, 0.9f, 1.0f, 1.0f });

  waveTickSprites_.clear();
  waveIconSprites_.clear();
  for (size_t i = 1; i < sectionProgresses.size(); ++i) {
    float ratio = sectionProgresses[i] / maxProgress;
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    float markerX = progressPos_.x + progressSize_.x * ratio;

    auto tick = std::make_unique<Sprite>();
    tick->Initialize("white.png");
    tick->SetAnchorPoint({ 0.5f, 0.5f });
    tick->SetTranslate({ markerX, 56.0f });
    tick->SetScale({ 2.0f, 20.0f });
    tick->SetColor({ 1.0f, 1.0f, 1.0f, 0.85f });
    waveTickSprites_.push_back(std::move(tick));

    auto icon = std::make_unique<Sprite>();
    icon->Initialize("ui/waveIconNormal.png");
    icon->SetAnchorPoint({ 0.5f, 0.5f });
    icon->SetTranslate({ markerX, 37.0f });
    icon->SetScale({ 20.0f, 20.0f });
    waveIconSprites_.push_back(std::move(icon));
  }

  bossWaveIconSprite_ = std::make_unique<Sprite>();
  bossWaveIconSprite_->Initialize("ui/waveIconBoss.png");
  bossWaveIconSprite_->SetAnchorPoint({ 0.5f, 0.5f });
  bossWaveIconSprite_->SetTranslate({ progressPos_.x + progressSize_.x + bossIconScale_ * 0.5f + bossIconSpacing_,
                                      progressPos_.y + progressSize_.y * 0.5f });
  bossWaveIconSprite_->SetScale({ bossIconScale_, bossIconScale_ });

  playerMarkerSprite_ = std::make_unique<Sprite>();
  playerMarkerSprite_->Initialize("ui/playerMarker.png");
  playerMarkerSprite_->SetAnchorPoint({ 0.5f, 0.5f });
  playerMarkerSprite_->SetTranslate({ 480.0f, 48.0f });
  playerMarkerSprite_->SetScale({ 16.0f, 16.0f });

  std::vector<bool> emptyCleared(sectionProgresses.size(), false);
  UpdateGameplayProgressBar(0.0f, maxProgress, sectionProgresses, emptyCleared, false, false, 0.0f);

  // 3. カバー演出UI
  coverOverlay_ = std::make_unique<Sprite>();
  coverOverlay_->Initialize("white.png");
  coverOverlay_->SetTranslate({ 0.0f, 470.0f });
  coverOverlay_->SetScale({ 1280.0f, 250.0f });
  coverOverlay_->SetColor({ 0.0f, 0.0f, 0.0f, 0.6f });

  // 4. スコアUI
  for (int i = 0; i < 10; ++i) {
    TextureManager::GetInstance()->LoadTexture("numbers/" + std::to_string(i) + ".png");
  }

  scoreBg_ = std::make_unique<Sprite>();
  scoreBg_->Initialize("white.png");
  scoreBg_->SetTranslate({ 30.0f, 30.0f });
  scoreBg_->SetScale({ 160.0f, 45.0f });
  scoreBg_->SetColor({ 0.2f, 0.2f, 0.2f, 0.8f });

  scoreUnits_.clear();
  for (int i = 0; i < 4; ++i) {
    auto unit = std::make_unique<Sprite>();
    unit->Initialize("numbers/0.png");
    unit->SetScale({ 20.0f, 32.0f });
    unit->SetTranslate({ 40.0f + i * 30.0f, 36.5f });
    scoreUnits_.push_back(std::move(unit));
  }

  // 5. 案内スプライト
  TextureManager::GetInstance()->LoadTexture("ui/escText.png");
  escTextSprite_ = std::make_unique<Sprite>();
  escTextSprite_->Initialize("ui/escText.png");
  escTextSprite_->SetTranslate(escTextPos_);
  escTextSprite_->SetScale(escTextScale_);

  TextureManager::GetInstance()->LoadTexture("ui/rText.png");
  rTextSprite_ = std::make_unique<Sprite>();
  rTextSprite_->Initialize("ui/rText.png");
  rTextSprite_->SetTranslate(rTextPos_);
  rTextSprite_->SetScale(rTextScale_);

  TextureManager::GetInstance()->LoadTexture("ui/control_gameScene.png");
  controlSprite_ = std::make_unique<Sprite>();
  controlSprite_->Initialize("ui/control_gameScene.png");
  controlSprite_->SetAnchorPoint({ 0.5f, 0.5f });
  controlSprite_->SetTranslate(controlPos_);
  controlSprite_->SetScale(controlScale_);

  // 6. Ready / Go 演出スプライト
  TextureManager::GetInstance()->LoadTexture("ui/readyText.png");
  TextureManager::GetInstance()->LoadTexture("ui/goText.png");

  readyGoRibbon_ = std::make_unique<Sprite>();
  readyGoRibbon_->Initialize("white.png");
  readyGoRibbon_->SetAnchorPoint({ 0.5f, 0.5f });
  readyGoRibbon_->SetScale(readyGoRibbonSize_);
  readyGoRibbon_->SetColor({ 0.12f, 0.12f, 0.12f, 0.70f });

  readyTextSprite_ = std::make_unique<Sprite>();
  readyTextSprite_->Initialize("ui/readyText.png");
  readyTextSprite_->SetAnchorPoint({ 0.5f, 0.5f });
  readyTextSprite_->SetScale(readyTextScale_);

  goTextSprite_ = std::make_unique<Sprite>();
  goTextSprite_->Initialize("ui/goText.png");
  goTextSprite_->SetAnchorPoint({ 0.5f, 0.5f });
  goTextSprite_->SetScale(goTextScale_);

  HideReadyGo();

  // 7. タイマーUI
  timerBg_ = std::make_unique<Sprite>();
  timerBg_->Initialize("white.png");
  timerBg_->SetTranslate(timerPos_);
  timerBg_->SetScale(timerBgSize_);
  timerBg_->SetColor({ 0.15f, 0.15f, 0.18f, 0.8f });

  timerUnits_.clear();
  for (int i = 0; i < 4; ++i) {
    auto unit = std::make_unique<Sprite>();
    unit->Initialize("numbers/0.png");
    unit->SetScale({ 20.0f, 32.0f });
    float x = 560.0f + i * 30.0f;
    if (i >= 2) {
      x += 7.0f;
    }
    unit->SetTranslate({ x, 75.0f });
    timerUnits_.push_back(std::move(unit));
  }

  timerDot_ = std::make_unique<Sprite>();
  timerDot_->Initialize("white.png");
  timerDot_->SetTranslate({ 618.0f, 100.5f });
  timerDot_->SetScale(timerDotSize_);
  UpdateGameplayTimer(initialGameTimer);
  timerDot_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
}

void ShootingHUD::UpdateReadyGo(float phaseTimer) {
  auto easeOutQuad = [](float p) { return 1.0f - (1.0f - p) * (1.0f - p); };
  auto easeInQuad = [](float p) { return p * p; };
  auto lerp = [](float a, float b, float p) { return a + (b - a) * p; };

  const float centerX = 640.0f;
  const float centerY = readyGoRibbonY_;
  const float ribbonRightOut = 1920.0f;
  const float ribbonLeftOut = -640.0f;
  const float textRightOut = 1500.0f;
  const float textLeftOut = -250.0f;

  isRibbonVisible_ = false;
  isReadyVisible_ = false;
  isGoVisible_ = false;

  if (phaseTimer < 0.35f) {
    float p = easeOutQuad(phaseTimer / 0.35f);
    if (readyGoRibbon_) readyGoRibbon_->SetTranslate({ lerp(ribbonRightOut, centerX, p), centerY });
    isRibbonVisible_ = true;
  } else if (phaseTimer < 1.95f) {
    if (readyGoRibbon_) readyGoRibbon_->SetTranslate({ centerX, centerY });
    isRibbonVisible_ = true;
  } else if (phaseTimer < 2.30f) {
    float p = easeInQuad((phaseTimer - 1.95f) / 0.35f);
    if (readyGoRibbon_) readyGoRibbon_->SetTranslate({ lerp(centerX, ribbonLeftOut, p), centerY });
    isRibbonVisible_ = true;
  }
  if (readyGoRibbon_) {
    readyGoRibbon_->SetScale(readyGoRibbonSize_);
    readyGoRibbon_->Update();
  }

  if (phaseTimer >= 0.20f && phaseTimer < 0.50f) {
    float p = easeOutQuad((phaseTimer - 0.20f) / 0.30f);
    if (readyTextSprite_) readyTextSprite_->SetTranslate({ lerp(textRightOut, centerX, p), centerY });
    isReadyVisible_ = true;
  } else if (phaseTimer >= 0.50f && phaseTimer < 1.05f) {
    if (readyTextSprite_) readyTextSprite_->SetTranslate({ centerX, centerY });
    isReadyVisible_ = true;
  } else if (phaseTimer >= 1.05f && phaseTimer < 1.30f) {
    float p = easeInQuad((phaseTimer - 1.05f) / 0.25f);
    if (readyTextSprite_) readyTextSprite_->SetTranslate({ lerp(centerX, textLeftOut, p), centerY });
    isReadyVisible_ = true;
  }
  if (readyTextSprite_) {
    readyTextSprite_->SetScale(readyTextScale_);
    readyTextSprite_->Update();
  }

  if (phaseTimer >= 1.25f && phaseTimer < 1.50f) {
    float p = easeOutQuad((phaseTimer - 1.25f) / 0.25f);
    if (goTextSprite_) goTextSprite_->SetTranslate({ lerp(textRightOut, centerX, p), centerY });
    isGoVisible_ = true;
  } else if (phaseTimer >= 1.50f && phaseTimer < 1.95f) {
    if (goTextSprite_) goTextSprite_->SetTranslate({ centerX, centerY });
    isGoVisible_ = true;
  } else if (phaseTimer >= 1.95f && phaseTimer < 2.30f) {
    float p = easeInQuad((phaseTimer - 1.95f) / 0.35f);
    if (goTextSprite_) goTextSprite_->SetTranslate({ lerp(centerX, textLeftOut, p), centerY });
    isGoVisible_ = true;
  }
  if (goTextSprite_) {
    goTextSprite_->SetScale(goTextScale_);
    goTextSprite_->Update();
  }
}

void ShootingHUD::HideReadyGo() {
  isRibbonVisible_ = false;
  isReadyVisible_ = false;
  isGoVisible_ = false;
}

void ShootingHUD::UpdateLifeGauge(int playerHp, int maxPlayerHp, float lifeBlinkTimer) {
  int clampedHp = (std::max)(0, (std::min)(playerHp, maxPlayerHp));
  int ones = clampedHp % 10;
  bool shouldBlink = (clampedHp > 0 && ones >= 1 && ones <= 5);
  int tipSegIndex = (clampedHp > 0) ? (clampedHp - 1) / 10 : -1;

  float blinkAlpha = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(lifeBlinkTimer * 16.0f));

  for (int i = 0; i < lifeSegments_; ++i) {
    if (i >= static_cast<int>(lifeUnits_.size()) || !lifeUnits_[i]) continue;
    int segHp = (std::max)(0, (std::min)(clampedHp - i * 10, 10));
    float segWidth = static_cast<float>(segHp) * 3.0f;
    lifeUnits_[i]->SetTranslate({ 1150.0f + i * 30.0f, 37.0f });
    lifeUnits_[i]->SetScale({ segWidth, 36.0f });

    if (shouldBlink && i == tipSegIndex) {
      lifeUnits_[i]->SetColor({ 1.0f, 0.4f, 0.0f, blinkAlpha });
    } else {
      lifeUnits_[i]->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
    }
    lifeUnits_[i]->Update();
  }
  for (auto& divider : lifeDividers_) {
    if (divider) divider->Update();
  }
  if (lifeBg_) lifeBg_->Update();
}

void ShootingHUD::ResetLifeGauge(int playerHp, int maxPlayerHp) {
  int clampedHp = (std::max)(0, (std::min)(playerHp, maxPlayerHp));
  for (int i = 0; i < lifeSegments_; ++i) {
    if (i >= static_cast<int>(lifeUnits_.size()) || !lifeUnits_[i]) continue;
    int segHp = (std::max)(0, (std::min)(clampedHp - i * 10, 10));
    lifeUnits_[i]->SetTranslate({ 1150.0f + i * 30.0f, 37.0f });
    lifeUnits_[i]->SetScale({ static_cast<float>(segHp) * 3.0f, 36.0f });
    lifeUnits_[i]->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
    lifeUnits_[i]->Update();
  }
}

void ShootingHUD::UpdateScoreAndGuides(int score, bool isCovering) {
  if (isCovering && coverOverlay_) {
    coverOverlay_->Update();
  }

  int tempScore = std::clamp(score, 0, 9999);
  int digits[4] = {
      tempScore / 1000,
      (tempScore % 1000) / 100,
      (tempScore % 100) / 10,
      tempScore % 10
  };

  for (int i = 0; i < 4; ++i) {
    if (i < static_cast<int>(scoreUnits_.size()) && scoreUnits_[i]) {
      scoreUnits_[i]->SetTexture("numbers/" + std::to_string(digits[i]) + ".png");
      scoreUnits_[i]->SetScale({ 20.0f, 32.0f });
      scoreUnits_[i]->Update();
    }
  }
  if (scoreBg_) scoreBg_->Update();
  if (escTextSprite_) escTextSprite_->Update();
  if (rTextSprite_) rTextSprite_->Update();
  if (controlSprite_) controlSprite_->Update();
}

void ShootingHUD::UpdateProgressBar(const Vector2& basePos, const Vector2& baseSize,
                                    float progress, float maxProgress,
                                    const std::vector<float>& sectionProgresses,
                                    const std::vector<bool>& sectionCleared,
                                    bool isSwarmBattle, bool isMovementPaused, float lifeBlinkTimer) {
  if (!progressBar_ || !progressBg_) return;

  float progressRatio = std::clamp(progress / maxProgress, 0.0f, 1.0f);

  progressBg_->SetTranslate(basePos);
  progressBg_->SetScale(baseSize);
  progressBg_->Update();

  progressBar_->SetTranslate(basePos);
  progressBar_->SetScale({ baseSize.x * progressRatio, baseSize.y });
  progressBar_->Update();

  for (size_t i = 0; i < waveIconSprites_.size(); ++i) {
    if (i + 1 >= sectionProgresses.size()) break;
    float waveDist = sectionProgresses[i + 1];
    float ratio = std::clamp(waveDist / maxProgress, 0.0f, 1.0f);
    float markerX = basePos.x + baseSize.x * ratio;

    if (i < waveTickSprites_.size() && waveTickSprites_[i]) {
      waveTickSprites_[i]->SetTranslate({ markerX, basePos.y + baseSize.y * 0.5f });
      waveTickSprites_[i]->SetScale({ 2.0f, baseSize.y + 4.0f });
      waveTickSprites_[i]->Update();
    }

    auto& icon = waveIconSprites_[i];
    if (icon) {
      icon->SetTranslate({ markerX, basePos.y - 11.0f });
      bool cleared = (i + 1 < sectionCleared.size()) ? sectionCleared[i + 1] : false;

      if (cleared && (progress >= waveDist || isSwarmBattle)) {
        icon->SetColor({ 0.45f, 0.5f, 0.55f, 0.65f });
        icon->SetScale({ 18.0f, 18.0f });
      } else if (std::abs(progress - waveDist) < 0.5f && isMovementPaused) {
        float pulse = 1.0f + 0.12f * std::sin(lifeBlinkTimer * 10.0f);
        icon->SetColor({ 1.0f, 0.35f, 0.1f, 1.0f });
        icon->SetScale({ 22.0f * pulse, 22.0f * pulse });
      } else {
        icon->SetColor({ 0.95f, 0.95f, 1.0f, 0.95f });
        icon->SetScale({ 20.0f, 20.0f });
      }
      icon->Update();
    }
  }

  float bossIconX = basePos.x + baseSize.x + bossIconScale_ * 0.5f + bossIconSpacing_;
  float bossIconY = basePos.y + baseSize.y * 0.5f;
  if (bossWaveIconSprite_) {
    bossWaveIconSprite_->SetTranslate({ bossIconX, bossIconY });
    if (isSwarmBattle) {
      float pulse = 1.0f + 0.15f * std::sin(lifeBlinkTimer * 12.0f);
      bossWaveIconSprite_->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f });
      bossWaveIconSprite_->SetScale({ bossIconScale_ * 1.15f * pulse, bossIconScale_ * 1.15f * pulse });
    } else {
      bossWaveIconSprite_->SetColor({ 1.0f, 0.85f, 0.85f, 1.0f });
      bossWaveIconSprite_->SetScale({ bossIconScale_, bossIconScale_ });
    }
    bossWaveIconSprite_->Update();
  }

  if (playerMarkerSprite_) {
    float currentMarkerX = basePos.x + baseSize.x * progressRatio;
    playerMarkerSprite_->SetTranslate({ currentMarkerX, bossIconY });
    playerMarkerSprite_->Update();
  }
}

void ShootingHUD::UpdateGameplayProgressBar(float progress, float maxProgress,
                                            const std::vector<float>& sectionProgresses,
                                            const std::vector<bool>& sectionCleared,
                                            bool isSwarmBattle, bool isMovementPaused, float lifeBlinkTimer) {
  UpdateProgressBar(progressPos_, progressSize_, progress, maxProgress,
                    sectionProgresses, sectionCleared, isSwarmBattle, isMovementPaused, lifeBlinkTimer);
}

void ShootingHUD::UpdateGameOverProgressBar(float progress, float maxProgress,
                                            const std::vector<float>& sectionProgresses,
                                            const std::vector<bool>& sectionCleared,
                                            float lifeBlinkTimer) {
  UpdateProgressBar(gameOverProgressPos_, gameOverProgressSize_, progress, maxProgress,
                    sectionProgresses, sectionCleared, false, false, lifeBlinkTimer);
}

void ShootingHUD::UpdateGameplayTimer(float gameTimer) {
  UpdateTimerInternal(gameTimer, timerPos_, timerBgSize_, timerDigitSize_, timerDigitSpacing_, timerDotSize_, true);
}

void ShootingHUD::UpdateGameOverTimer(float gameOverTimer) {
  UpdateTimerInternal(gameOverTimer, gameOverTimerPos_, gameOverTimerBgSize_, gameOverTimerDigitSize_, gameOverTimerDigitSpacing_, gameOverTimerDotSize_, false);
}

void ShootingHUD::UpdateTimerInternal(float timeValue, const Vector2& basePos, const Vector2& bgSize,
                                      const Vector2& digitSize, float spacing, const Vector2& dotSize, bool showDecimals) {
  if (!timerBg_) return;

  timerBg_->SetTranslate(basePos);
  timerBg_->SetScale(bgSize);
  timerBg_->Update();

  if (showDecimals) {
    int tempTimer = std::clamp(static_cast<int>(std::round(timeValue * 100.0f)), 0, 9999);
    int tDigits[4] = {
        tempTimer / 1000,
        (tempTimer % 1000) / 100,
        (tempTimer % 100) / 10,
        tempTimer % 10
    };

    float dotGap = spacing * 0.25f;
    float totalContentWidth = digitSize.x + spacing * 3.0f + dotGap;
    float startX = basePos.x + (bgSize.x - totalContentWidth) * 0.5f;
    float startY = basePos.y + (bgSize.y - digitSize.y) * 0.5f;

    for (int i = 0; i < 4; ++i) {
      if (i < static_cast<int>(timerUnits_.size()) && timerUnits_[i]) {
        timerUnits_[i]->SetTexture("numbers/" + std::to_string(tDigits[i]) + ".png");
        float x = startX + i * spacing + (i >= 2 ? dotGap : 0.0f);
        timerUnits_[i]->SetTranslate({ x, startY });
        timerUnits_[i]->SetScale(digitSize);
        timerUnits_[i]->Update();
      }
    }

    if (timerDot_) {
      float dotX = startX + 2.0f * spacing - (dotGap * 0.5f) - (dotSize.x * 0.5f);
      float dotY = startY + digitSize.y - dotSize.y * 1.5f;
      timerDot_->SetTranslate({ dotX, dotY });
      timerDot_->SetScale(dotSize);
      timerDot_->Update();
    }
  } else {
    int sec = std::clamp(static_cast<int>(std::ceil(timeValue)), 0, 99);
    int tDigits[2] = { sec / 10, sec % 10 };
    float totalContentWidth = digitSize.x + spacing;
    float startX = basePos.x + (bgSize.x - totalContentWidth) * 0.5f;
    float startY = basePos.y + (bgSize.y - digitSize.y) * 0.5f;
    for (int i = 0; i < 2; ++i) {
      if (i < static_cast<int>(timerUnits_.size()) && timerUnits_[i]) {
        timerUnits_[i]->SetTexture("numbers/" + std::to_string(tDigits[i]) + ".png");
        timerUnits_[i]->SetTranslate({ startX + i * spacing, startY });
        timerUnits_[i]->SetScale(digitSize);
        timerUnits_[i]->Update();
      }
    }
    for (size_t i = 2; i < timerUnits_.size(); ++i) {
      if (timerUnits_[i]) {
        timerUnits_[i]->SetScale({ 0.0f, 0.0f });
      }
    }
    if (timerDot_) {
      timerDot_->SetScale({ 0.0f, 0.0f });
    }
  }
}

void ShootingHUD::DrawCover(bool isCovering) {
  if (isCovering && coverOverlay_) {
    coverOverlay_->Draw();
  }
}

void ShootingHUD::DrawGameplayBackgrounds() {
  if (lifeBg_) lifeBg_->Draw();
  if (progressBg_) progressBg_->Draw();
  if (scoreBg_) scoreBg_->Draw();
  if (timerBg_) timerBg_->Draw();
}

void ShootingHUD::DrawGameOverBackgrounds() {
  if (progressBg_) progressBg_->Draw();
  if (timerBg_) timerBg_->Draw();
}

void ShootingHUD::DrawLifeGauge(int playerHp, int maxPlayerHp) {
  int clampedHp = (std::max)(0, (std::min)(playerHp, maxPlayerHp));
  for (int i = 0; i < lifeSegments_; ++i) {
    int segHp = (std::max)(0, (std::min)(clampedHp - i * 10, 10));
    if (segHp > 0 && i < static_cast<int>(lifeUnits_.size()) && lifeUnits_[i]) {
      lifeUnits_[i]->Draw();
    }
  }
  for (const auto& divider : lifeDividers_) {
    if (divider) divider->Draw();
  }
}

void ShootingHUD::DrawProgressBar() {
  if (progressBar_) progressBar_->Draw();
  for (const auto& tick : waveTickSprites_) {
    if (tick) tick->Draw();
  }
  for (const auto& icon : waveIconSprites_) {
    if (icon) icon->Draw();
  }
  if (bossWaveIconSprite_) bossWaveIconSprite_->Draw();
  if (playerMarkerSprite_) playerMarkerSprite_->Draw();
}

void ShootingHUD::DrawScoreAndGuides() {
  for (const auto& unit : scoreUnits_) {
    if (unit) unit->Draw();
  }
  if (escTextSprite_) escTextSprite_->Draw();
  if (rTextSprite_) rTextSprite_->Draw();
  if (controlSprite_) controlSprite_->Draw();
}

void ShootingHUD::DrawTimer(bool showDecimals) {
  int count = showDecimals ? 4 : 2;
  for (int i = 0; i < count && i < static_cast<int>(timerUnits_.size()); ++i) {
    if (timerUnits_[i]) timerUnits_[i]->Draw();
  }
  if (showDecimals && timerDot_) {
    timerDot_->Draw();
  }
}

void ShootingHUD::DrawReadyGo() {
  if (readyGoRibbon_ && isRibbonVisible_) readyGoRibbon_->Draw();
  if (readyTextSprite_ && isReadyVisible_) readyTextSprite_->Draw();
  if (goTextSprite_ && isGoVisible_) goTextSprite_->Draw();
}

#ifdef USE_IMGUI
void ShootingHUD::UpdateGuidesImGui() {
  if (ImGui::TreeNode("ESC Text UI")) {
    bool changed = false;
    changed |= ImGui::DragFloat2("Position", &escTextPos_.x, 1.0f);
    changed |= ImGui::DragFloat2("Scale", &escTextScale_.x, 1.0f);
    if (changed && escTextSprite_) {
      escTextSprite_->SetTranslate(escTextPos_);
      escTextSprite_->SetScale(escTextScale_);
    }
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("R Key Text UI")) {
    bool changed = false;
    changed |= ImGui::DragFloat2("Position", &rTextPos_.x, 1.0f);
    changed |= ImGui::DragFloat2("Scale", &rTextScale_.x, 1.0f);
    if (changed && rTextSprite_) {
      rTextSprite_->SetTranslate(rTextPos_);
      rTextSprite_->SetScale(rTextScale_);
    }
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Control UI")) {
    bool changed = false;
    changed |= ImGui::DragFloat2("Position", &controlPos_.x, 1.0f);
    changed |= ImGui::DragFloat2("Scale", &controlScale_.x, 1.0f);
    if (changed && controlSprite_) {
      controlSprite_->SetTranslate(controlPos_);
      controlSprite_->SetScale(controlScale_);
    }
    ImGui::TreePop();
  }
}

bool ShootingHUD::UpdateReadyGoImGui() {
  ImGui::Separator();
  ImGui::Text("Ready / Go Banner Settings");
  ImGui::SliderFloat("Banner Center Y", &readyGoRibbonY_, 100.0f, 620.0f, "%.1f");
  ImGui::SliderFloat2("Banner Size", &readyGoRibbonSize_.x, 200.0f, 1920.0f, "%.0f");
  ImGui::SliderFloat2("Ready Text Scale", &readyTextScale_.x, 100.0f, 600.0f, "%.0f");
  ImGui::SliderFloat2("Go Text Scale", &goTextScale_.x, 100.0f, 600.0f, "%.0f");
  return ImGui::Button("Play Ready-Go Only");
}

void ShootingHUD::UpdateProgressBarImGui() {
  if (ImGui::TreeNode("Progress Bar Layout")) {
    ImGui::Text("Normal (Playing):");
    ImGui::DragFloat2("Pos##ProgNorm", &progressPos_.x, 1.0f);
    ImGui::DragFloat2("Size##ProgNorm", &progressSize_.x, 1.0f);
    ImGui::DragFloat("Boss Size##ProgNorm", &bossIconScale_, 1.0f, 12.0f, 48.0f, "%.1f px");
    ImGui::DragFloat("Boss Spacing##ProgNorm", &bossIconSpacing_, 1.0f, -20.0f, 50.0f, "%.1f px");
    ImGui::Separator();
    ImGui::Text("GameOver:");
    ImGui::DragFloat2("Pos##ProgGO", &gameOverProgressPos_.x, 1.0f);
    ImGui::DragFloat2("Size##ProgGO", &gameOverProgressSize_.x, 1.0f);
    ImGui::TreePop();
  }
}

void ShootingHUD::UpdateTimerImGui(float& gameOverTimer) {
  if (ImGui::TreeNode("Timer Layout")) {
    ImGui::Text("Normal (Playing):");
    ImGui::DragFloat2("Pos##TimerNorm", &timerPos_.x, 1.0f);
    ImGui::DragFloat2("BgSize##TimerNorm", &timerBgSize_.x, 1.0f);
    ImGui::DragFloat2("DigitSize##TimerNorm", &timerDigitSize_.x, 1.0f);
    ImGui::DragFloat("Spacing##TimerNorm", &timerDigitSpacing_, 1.0f);
    ImGui::DragFloat2("DotSize##TimerNorm", &timerDotSize_.x, 1.0f);
    ImGui::Separator();
    ImGui::Text("GameOver:");
    ImGui::DragFloat2("Pos##TimerGO", &gameOverTimerPos_.x, 1.0f);
    ImGui::DragFloat2("BgSize##TimerGO", &gameOverTimerBgSize_.x, 1.0f);
    ImGui::DragFloat2("DigitSize##TimerGO", &gameOverTimerDigitSize_.x, 1.0f);
    ImGui::DragFloat("Spacing##TimerGO", &gameOverTimerDigitSpacing_, 1.0f);
    ImGui::DragFloat2("DotSize##TimerGO", &gameOverTimerDotSize_.x, 1.0f);
    ImGui::DragFloat("Timer Value", &gameOverTimer, 0.1f, 0.0f, 60.0f);
    ImGui::TreePop();
  }
}
#endif

void ShootingHUD::ApplyGameOverFade(float remainAlpha) {
  if (timerBg_) {
    timerBg_->SetColor({ 0.15f, 0.15f, 0.18f, 0.8f * remainAlpha });
  }
  for (auto& unit : timerUnits_) {
    if (unit) {
      unit->SetColor({ 1.0f, 1.0f, 1.0f, remainAlpha });
    }
  }
  if (timerDot_) {
    timerDot_->SetColor({ 1.0f, 1.0f, 1.0f, remainAlpha });
  }
  if (progressBg_) {
    progressBg_->SetColor({ 0.1f, 0.15f, 0.35f, 0.75f * remainAlpha });
  }
  if (progressBar_) {
    progressBar_->SetColor({ 0.2f, 0.9f, 1.0f, remainAlpha });
  }
  for (size_t i = 0; i < waveTickSprites_.size(); ++i) {
    if (waveTickSprites_[i]) {
      if (i == waveTickSprites_.size() - 1) {
        waveTickSprites_[i]->SetColor({ 1.0f, 0.3f, 0.3f, 0.9f * remainAlpha });
      } else {
        waveTickSprites_[i]->SetColor({ 1.0f, 1.0f, 1.0f, 0.85f * remainAlpha });
      }
    }
  }
  for (auto& icon : waveIconSprites_) {
    if (icon) {
      Vector4 col = icon->GetColor();
      col.w *= remainAlpha;
      icon->SetColor(col);
    }
  }
  if (bossWaveIconSprite_) {
    Vector4 col = bossWaveIconSprite_->GetColor();
    col.w *= remainAlpha;
    bossWaveIconSprite_->SetColor(col);
  }
  if (playerMarkerSprite_) {
    playerMarkerSprite_->SetColor({ 1.0f, 1.0f, 1.0f, remainAlpha });
  }
}

void ShootingHUD::ResetGameOverColors() {
  if (timerBg_) {
    timerBg_->SetColor({ 0.15f, 0.15f, 0.18f, 0.8f });
  }
  for (auto& unit : timerUnits_) {
    if (unit) {
      unit->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }
  }
  if (timerDot_) {
    timerDot_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
  }
  if (progressBg_) {
    progressBg_->SetColor({ 0.1f, 0.15f, 0.35f, 0.75f });
  }
  if (progressBar_) {
    progressBar_->SetColor({ 0.2f, 0.9f, 1.0f, 1.0f });
  }
  for (size_t i = 0; i < waveTickSprites_.size(); ++i) {
    if (waveTickSprites_[i]) {
      if (i == waveTickSprites_.size() - 1) {
        waveTickSprites_[i]->SetColor({ 1.0f, 0.3f, 0.3f, 0.9f });
      } else {
        waveTickSprites_[i]->SetColor({ 1.0f, 1.0f, 1.0f, 0.85f });
      }
    }
  }
  if (playerMarkerSprite_) {
    playerMarkerSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
  }
}
