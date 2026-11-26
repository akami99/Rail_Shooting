#pragma once

#include <d3d12.h>
#include <memory>
#include <string>
#include <algorithm>
#include <unordered_map>
#include "IPostEffect.h"
#include "PostEffects.h"

class RenderTexture;

/// @brief ポストプロセス（グレースケール・セピア等）の切り替えと
///        IPostEffect 派生クラス群の管理を行うシングルトン管理クラス
class PostProcessManager {
public:
#pragma region ポストエフェクトタイプ
    // --- ポストエフェクトモード定数 ---
    static constexpr int kModeCopy         = 0; // パススルー（無効果）
    static constexpr int kModeGrayscale    = 1;
    static constexpr int kModeSepia        = 2;
    static constexpr int kModeVignette     = 3;
    static constexpr int kModeSmoothing    = 4;
    static constexpr int kModeGaussianBlur = 5;
    static constexpr int kModeOutline      = 6;
    static constexpr int kModeRadialBlur   = 7;
    static constexpr int kModeDissolve     = 8;
    static constexpr int kModeRandom       = 9;
    static constexpr int kModeHSV          = 10;
#pragma endregion

    // ---- Passkey Idiom ----
    struct Token {
    private:
        friend class PostProcessManager;
        Token() {}
    };

#pragma region 制限範囲定数
    // --- パラメータ制限用の静的メンバ定数 ---
    static constexpr float kMinVignetteScale = 0.0f;
    static constexpr float kMaxVignetteScale = 32.0f;
    static constexpr float kMinVignetteExponent = 0.0f;
    static constexpr float kMaxVignetteExponent = 5.0f;

    static constexpr int kMinSmoothingKernelSize = 3;
    static constexpr int kMaxSmoothingKernelSize = 15;

    static constexpr int kMinGaussianBlurKernelSize = 3;
    static constexpr int kMaxGaussianBlurKernelSize = 15;
    static constexpr float kMinGaussianBlurSigma = 0.1f;
    static constexpr float kMaxGaussianBlurSigma = 10.0f;

    static constexpr float kMinOutlineDepthEdgeMultiplier = 0.0f;
    static constexpr float kMaxOutlineDepthEdgeMultiplier = 20.0f;
    static constexpr float kMinOutlineColorEdgeMultiplier = 0.0f;
    static constexpr float kMaxOutlineColorEdgeMultiplier = 5.0f;

    static constexpr float kMinRadialBlurCenterX = 0.0f;
    static constexpr float kMaxRadialBlurCenterX = 1.0f;
    static constexpr float kMinRadialBlurCenterY = 0.0f;
    static constexpr float kMaxRadialBlurCenterY = 1.0f;
    static constexpr float kMinRadialBlurWidth = 0.0f;
    static constexpr float kMaxRadialBlurWidth = 0.1f;
    static constexpr int kMinRadialBlurSampleCount = 2;
    static constexpr int kMaxRadialBlurSampleCount = 50;

    static constexpr float kMinDissolveThreshold = 0.0f;
    static constexpr float kMaxDissolveThreshold = 1.0f;
    static constexpr float kMinDissolveEdgeRange = 0.0f;
    static constexpr float kMaxDissolveEdgeRange = 0.2f;

    static constexpr float kMinRandomStrength = 0.0f;
    static constexpr float kMaxRandomStrength = 1.0f;

    static constexpr float kMinHSVHue = -1.0f;
    static constexpr float kMaxHSVHue = 1.0f;
    static constexpr float kMinHSVSaturation = -1.0f;
    static constexpr float kMaxHSVSaturation = 1.0f;
    static constexpr float kMinHSVValue = -1.0f;
    static constexpr float kMaxHSVValue = 1.0f;
#pragma endregion

private:
    std::unordered_map<int, std::unique_ptr<IPostEffect>> effects_;

    // 各エフェクトへの非所有ポインタ（パラメータ設定委譲用）
    ColorFilterPostEffect* grayscaleEffect_ = nullptr;
    ColorFilterPostEffect* sepiaEffect_ = nullptr;
    VignettePostEffect* vignetteEffect_ = nullptr;
    SmoothingPostEffect* smoothingEffect_ = nullptr;
    GaussianBlurPostEffect* gaussianBlurEffect_ = nullptr;
    OutlinePostEffect* outlineEffect_ = nullptr;
    RadialBlurPostEffect* radialBlurEffect_ = nullptr;
    DissolvePostEffect* dissolveEffect_ = nullptr;
    RandomPostEffect* randomEffect_ = nullptr;
    HSVPostEffect* hsvEffect_ = nullptr;

    uint32_t depthSrvIndex_ = 0;

    int modeNext_ = kModeCopy;
    int currentMode_ = kModeCopy;

    static std::unique_ptr<PostProcessManager> instance_;

    template <typename T, typename... Args>
    T* RegisterEffect(int mode, Args&&... args) {
        auto effect = std::make_unique<T>(std::forward<Args>(args)...);
        T* rawPtr = effect.get();
        rawPtr->Initialize();
        effects_[mode] = std::move(effect);
        return rawPtr;
    }

    IPostEffect* FindEffect(int mode) const;

public:
    static PostProcessManager* GetInstance();
    static void Destroy();

    explicit PostProcessManager(Token) {}

    /// <summary>
    /// 全ポストエフェクトと深度SRVを初期化する
    /// </summary>
    void Initialize();

    /// <summary>
    /// 画面全体にポストエフェクトを描画する
    /// </summary>
    /// <param name="renderTexture">オフスクリーンレンダリング結果</param>
    void Draw(RenderTexture* renderTexture);

    /// <summary>
    /// 更新処理を行う（ロジックの更新）
    /// </summary>
    /// <param name="deltaTime">経過時間</param>
    void Update(float deltaTime);

    // --- セッター (外部から呼び出す) ---

    void SetMode(int mode) { modeNext_ = mode; }

    void SetStrength(float s) {
        if (grayscaleEffect_) grayscaleEffect_->SetStrength(s);
        if (sepiaEffect_) sepiaEffect_->SetStrength(s);
    }

    void SetVignetteParams(float scale, float exponent) {
        if (vignetteEffect_) {
            vignetteEffect_->SetParams(
                std::clamp(scale, kMinVignetteScale, kMaxVignetteScale),
                std::clamp(exponent, kMinVignetteExponent, kMaxVignetteExponent));
        }
    }

    void SetSmoothingParams(int kernelSize) {
        if (smoothingEffect_) {
            int clampedSize = std::clamp(kernelSize, kMinSmoothingKernelSize, kMaxSmoothingKernelSize);
            if (clampedSize % 2 == 0) {
                clampedSize += 1;
            }
            smoothingEffect_->SetParams(clampedSize);
        }
    }

    void SetGaussianBlurParams(int kernelSize, float sigma) {
        if (gaussianBlurEffect_) {
            int clampedSize = std::clamp(kernelSize, kMinGaussianBlurKernelSize, kMaxGaussianBlurKernelSize);
            if (clampedSize % 2 == 0) {
                clampedSize += 1;
            }
            gaussianBlurEffect_->SetParams(
                clampedSize,
                std::clamp(sigma, kMinGaussianBlurSigma, kMaxGaussianBlurSigma));
        }
    }

    void SetOutlineParams(float depthEdgeMultiplier, float colorEdgeMultiplier) {
        if (outlineEffect_) {
            outlineEffect_->SetParams(
                std::clamp(depthEdgeMultiplier, kMinOutlineDepthEdgeMultiplier, kMaxOutlineDepthEdgeMultiplier),
                std::clamp(colorEdgeMultiplier, kMinOutlineColorEdgeMultiplier, kMaxOutlineColorEdgeMultiplier));
        }
    }

    void SetRadialBlurParams(float centerX, float centerY, float width, int sampleCount) {
        if (radialBlurEffect_) {
            radialBlurEffect_->SetParams(
                std::clamp(centerX, kMinRadialBlurCenterX, kMaxRadialBlurCenterX),
                std::clamp(centerY, kMinRadialBlurCenterY, kMaxRadialBlurCenterY),
                std::clamp(width, kMinRadialBlurWidth, kMaxRadialBlurWidth),
                std::clamp(sampleCount, kMinRadialBlurSampleCount, kMaxRadialBlurSampleCount));
        }
    }

    void SetDissolveParams(const float edgeColor[3], float threshold, float edgeRange) {
        if (dissolveEffect_) {
            float clampedColor[3] = {
                std::clamp(edgeColor[0], 0.0f, 1.0f),
                std::clamp(edgeColor[1], 0.0f, 1.0f),
                std::clamp(edgeColor[2], 0.0f, 1.0f)
            };
            dissolveEffect_->SetParams(
                clampedColor,
                std::clamp(threshold, kMinDissolveThreshold, kMaxDissolveThreshold),
                std::clamp(edgeRange, kMinDissolveEdgeRange, kMaxDissolveEdgeRange));
        }
    }

    void SetDissolveMaskTexture(const std::string& maskPath) {
        if (dissolveEffect_) {
            dissolveEffect_->SetMaskTexture(maskPath);
        }
    }

    void SetRandomParams(float strength) {
        if (randomEffect_) {
            randomEffect_->SetParams(std::clamp(strength, kMinRandomStrength, kMaxRandomStrength));
        }
    }

    void SetHSVParams(float hue, float saturation, float value) {
        if (hsvEffect_) {
            hsvEffect_->SetParams(
                std::clamp(hue, kMinHSVHue, kMaxHSVHue),
                std::clamp(saturation, kMinHSVSaturation, kMaxHSVSaturation),
                std::clamp(value, kMinHSVValue, kMaxHSVValue));
        }
    }

    // --- パラメータ取得用ゲッター (双方向同期用) ---
    float GetVignetteScale() const { return vignetteEffect_ ? vignetteEffect_->GetScale() : 16.0f; }
    float GetVignetteExponent() const { return vignetteEffect_ ? vignetteEffect_->GetExponent() : 0.8f; }
    int GetSmoothingKernelSize() const { return smoothingEffect_ ? smoothingEffect_->GetKernelSize() : 3; }
    int GetGaussianBlurKernelSize() const { return gaussianBlurEffect_ ? gaussianBlurEffect_->GetKernelSize() : 3; }
    float GetGaussianBlurSigma() const { return gaussianBlurEffect_ ? gaussianBlurEffect_->GetSigma() : 2.0f; }
    float GetOutlineDepthEdgeMultiplier() const { return outlineEffect_ ? outlineEffect_->GetDepthEdgeMultiplier() : 6.0f; }
    float GetOutlineColorEdgeMultiplier() const { return outlineEffect_ ? outlineEffect_->GetColorEdgeMultiplier() : 1.0f; }
    float GetRadialBlurCenterX() const { return radialBlurEffect_ ? radialBlurEffect_->GetCenterX() : 0.5f; }
    float GetRadialBlurCenterY() const { return radialBlurEffect_ ? radialBlurEffect_->GetCenterY() : 0.5f; }
    float GetRadialBlurWidth() const { return radialBlurEffect_ ? radialBlurEffect_->GetWidth() : 0.01f; }
    int GetRadialBlurSampleCount() const { return radialBlurEffect_ ? radialBlurEffect_->GetSampleCount() : 10; }
    float GetDissolveThreshold() const { return dissolveEffect_ ? dissolveEffect_->GetThreshold() : 0.0f; }
    float GetDissolveEdgeRange() const { return dissolveEffect_ ? dissolveEffect_->GetEdgeRange() : 0.02f; }
    const float* GetDissolveEdgeColor() const { return dissolveEffect_ ? dissolveEffect_->GetEdgeColor() : nullptr; }
    float GetRandomStrength() const { return randomEffect_ ? randomEffect_->GetStrength() : 0.0f; }
    float GetHSVHue() const { return hsvEffect_ ? hsvEffect_->GetHue() : 0.0f; }
    float GetHSVSaturation() const { return hsvEffect_ ? hsvEffect_->GetSaturation() : 0.0f; }
    float GetHSVValue() const { return hsvEffect_ ? hsvEffect_->GetValue() : 0.0f; }

    int GetCurrentMode() const { return currentMode_; }

private:
    ~PostProcessManager() = default;
    PostProcessManager(const PostProcessManager&) = delete;
    PostProcessManager& operator=(const PostProcessManager&) = delete;
    friend std::default_delete<PostProcessManager>;
};
