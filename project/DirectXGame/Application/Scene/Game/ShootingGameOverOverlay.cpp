#include "ShootingGameOverOverlay.h"
#include "ApplicationConfig.h"
#include "PostProcessManager.h"
#include "ShootingHUD.h"
#include "ShootingPlayerController.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include "imgui.h"
#endif // USE_IMGUI

void ShootingGameOverOverlay::Initialize() {
  TextureManager::GetInstance()->LoadTexture("gameover/selectRetry.png");
  TextureManager::GetInstance()->LoadTexture("gameover/selectTitle.png");
  TextureManager::GetInstance()->LoadTexture("gameover/NormalBullet.png");
  TextureManager::GetInstance()->LoadTexture("gameover/EngineerRocket.png");
  TextureManager::GetInstance()->LoadTexture("gameover/CoverBlast.png");
  TextureManager::GetInstance()->LoadTexture("gameover/TimeUp.png");
  TextureManager::GetInstance()->LoadTexture("gameover/DroneExplosion.png");

  retrySprite_ = std::make_unique<Sprite>();
  retrySprite_->Initialize("gameover/selectRetry.png");
  retrySprite_->SetAnchorPoint({ 0.5f, 0.5f });
  retrySprite_->SetTranslate(retryPos_);
  retrySprite_->SetScale(retryScale_);
  retrySprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.4f });

  titleSprite_ = std::make_unique<Sprite>();
  titleSprite_->Initialize("gameover/selectTitle.png");
  titleSprite_->SetAnchorPoint({ 0.5f, 0.5f });
  titleSprite_->SetTranslate(titlePos_);
  titleSprite_->SetScale(titleScale_);
  titleSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.4f });

  deathReasonSprite_ = std::make_unique<Sprite>();
  deathReasonSprite_->Initialize("gameover/NormalBullet.png");
  deathReasonSprite_->SetAnchorPoint({ 0.5f, 0.5f });
  deathReasonSprite_->SetTranslate(deathReasonPos_);
  deathReasonSprite_->SetScale(deathReasonScale_);
  deathReasonSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

  selectedOption_ = SelectOption::None;
  transitionTimer_ = 0.0f;
}

void ShootingGameOverOverlay::Begin(DeathReason reason, float& outGameOverTimer) {
  std::string reasonTexturePath = "gameover/NormalBullet.png";
  switch (reason) {
  case DeathReason::NormalBullet:
    reasonTexturePath = "gameover/NormalBullet.png";
    break;
  case DeathReason::EngineerRocket:
    reasonTexturePath = "gameover/EngineerRocket.png";
    break;
  case DeathReason::CoverBlast:
    reasonTexturePath = "gameover/CoverBlast.png";
    break;
  case DeathReason::TimeUp:
    reasonTexturePath = "gameover/TimeUp.png";
    break;
  case DeathReason::DroneExplosion:
    reasonTexturePath = "gameover/DroneExplosion.png";
    break;
  }

  if (deathReasonSprite_) {
    deathReasonSprite_->SetTexture(reasonTexturePath);
    deathReasonSprite_->SetTranslate(deathReasonPos_);
    deathReasonSprite_->SetScale(deathReasonScale_);
    deathReasonSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    deathReasonSprite_->Update();
  }

  deathReason_ = reason;
  outGameOverTimer = 20.0f;
  selectedOption_ = SelectOption::None;
  transitionTimer_ = 0.0f;

  PostProcessManager::GetInstance()->SetMode(PostProcessManager::kModeVignette);
  PostProcessManager::GetInstance()->SetStrength(1.0f);
  PostProcessManager::GetInstance()->SetVignetteParams(16.0f, 0.8f);
}

ShootingGameOverOverlay::UpdateResult ShootingGameOverOverlay::Update(
    float phaseTimer, float& gameOverTimer, const Vector2& mousePos, bool isClick,
    ShootingHUD* hud, ShootingPlayerController* playerController) {
  UpdateResult result;

  if (selectedOption_ == SelectOption::None) {
    gameOverTimer -= ApplicationConfig::kDeltaTime;
    if (gameOverTimer <= 0.0f) {
      gameOverTimer = 0.0f;
      selectedOption_ = SelectOption::Title;
      transitionTimer_ = 0.0f;
    }
  }

  float t = (std::min)(1.0f, phaseTimer / 1.0f);
  float vignetteScale = 16.0f;
  float vignetteExponent = 0.8f;
  if (selectedOption_ == SelectOption::None) {
    vignetteScale = 16.0f + (5.0f - 16.0f) * t;
    vignetteExponent = 0.8f + (1.5f - 0.8f) * t;
  } else {
    vignetteScale = 5.0f;
    vignetteExponent = 1.5f;
    if (transitionTimer_ >= kGameOverBlinkDuration) {
      float fadeT = (std::min)(1.0f, (transitionTimer_ - kGameOverBlinkDuration) / kGameOverFadeOutDuration);
      vignetteScale = 5.0f * (1.0f - fadeT);
      vignetteExponent = 1.5f + (6.0f - 1.5f) * fadeT;
    }
  }
  PostProcessManager::GetInstance()->SetVignetteParams(vignetteScale, vignetteExponent);

  if (retrySprite_ && titleSprite_) {
    retrySprite_->SetTranslate(retryPos_);
    retrySprite_->SetScale(retryScale_);
    titleSprite_->SetTranslate(titlePos_);
    titleSprite_->SetScale(titleScale_);

    if (selectedOption_ == SelectOption::None) {
      bool isHoverRetry = IsPointInRect(mousePos, retryPos_, retryScale_);
      bool isHoverTitle = IsPointInRect(mousePos, titlePos_, titleScale_);

      retrySprite_->SetColor({ 1.0f, 1.0f, 1.0f, isHoverRetry ? 1.0f : 0.4f });
      titleSprite_->SetColor({ 1.0f, 1.0f, 1.0f, isHoverTitle ? 1.0f : 0.4f });

      result.updateCrosshair = true;
      result.hoverAny = (isHoverRetry || isHoverTitle);

      if (isClick) {
        if (isHoverRetry) {
          selectedOption_ = SelectOption::Retry;
          transitionTimer_ = 0.0f;
        } else if (isHoverTitle) {
          selectedOption_ = SelectOption::Title;
          transitionTimer_ = 0.0f;
        }
      }
    } else {
      transitionTimer_ += ApplicationConfig::kDeltaTime;

      if (transitionTimer_ < kGameOverBlinkDuration) {
        bool isBlinkOn = std::sin(transitionTimer_ * 30.0f) > 0.0f;
        float blinkAlpha = isBlinkOn ? 1.0f : 0.0f;

        if (selectedOption_ == SelectOption::Retry) {
          retrySprite_->SetColor({ 1.0f, 1.0f, 1.0f, blinkAlpha });
          titleSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.1f });
        } else {
          titleSprite_->SetColor({ 1.0f, 1.0f, 1.0f, blinkAlpha });
          retrySprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.1f });
        }
      } else {
        float fadeElapsed = transitionTimer_ - kGameOverBlinkDuration;
        float fadeT = (std::min)(1.0f, fadeElapsed / kGameOverFadeOutDuration);
        float remainAlpha = 1.0f - fadeT;

        if (hud) {
          hud->ApplyGameOverFade(remainAlpha);
        }
        if (deathReasonSprite_) {
          deathReasonSprite_->SetColor({ 1.0f, 1.0f, 1.0f, remainAlpha });
        }
        if (selectedOption_ == SelectOption::Retry) {
          retrySprite_->SetColor({ 1.0f, 1.0f, 1.0f, remainAlpha });
          titleSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        } else {
          titleSprite_->SetColor({ 1.0f, 1.0f, 1.0f, remainAlpha });
          retrySprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        }
        if (playerController) {
          playerController->FadeCrosshair(remainAlpha);
        }

        if (fadeElapsed >= (kGameOverFadeOutDuration + kGameOverWaitAfterFade)) {
          if (selectedOption_ == SelectOption::Retry) {
            result.requestRetry = true;
          } else {
            result.requestTitle = true;
          }
          return result;
        }
      }
    }

    retrySprite_->Update();
    titleSprite_->Update();
    if (deathReasonSprite_) {
      deathReasonSprite_->Update();
    }
  }

  return result;
}

void ShootingGameOverOverlay::Draw() {
  if (retrySprite_) {
    retrySprite_->Draw();
  }
  if (titleSprite_) {
    titleSprite_->Draw();
  }
  if (deathReasonSprite_) {
    deathReasonSprite_->Draw();
  }
}

bool ShootingGameOverOverlay::IsPointInRect(const Vector2& point, const Vector2& center, const Vector2& size) {
  float halfW = size.x * 0.5f;
  float halfH = size.y * 0.5f;
  return (point.x >= center.x - halfW && point.x <= center.x + halfW &&
          point.y >= center.y - halfH && point.y <= center.y + halfH);
}

#ifdef USE_IMGUI
void ShootingGameOverOverlay::UpdateImGui() {
  if (ImGui::TreeNode("Retry/Title Select UI")) {
    ImGui::DragFloat2("Retry Pos", &retryPos_.x, 1.0f);
    ImGui::DragFloat2("Retry Scale", &retryScale_.x, 1.0f);
    ImGui::Separator();
    ImGui::DragFloat2("Title Pos", &titlePos_.x, 1.0f);
    ImGui::DragFloat2("Title Scale", &titleScale_.x, 1.0f);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("GameOver Death Reason UI")) {
    bool changed = false;
    changed |= ImGui::DragFloat2("Pos##DeathReason", &deathReasonPos_.x, 1.0f);
    changed |= ImGui::DragFloat2("Scale##DeathReason", &deathReasonScale_.x, 1.0f);
    if (changed && deathReasonSprite_) {
      deathReasonSprite_->SetTranslate(deathReasonPos_);
      deathReasonSprite_->SetScale(deathReasonScale_);
    }
    ImGui::TreePop();
  }
}
#endif // USE_IMGUI
