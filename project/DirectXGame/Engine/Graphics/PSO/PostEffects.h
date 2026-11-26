#pragma once

#include "IPostEffect.h"
#include "Types/GraphicsTypes.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

/// @brief パススルー（無効果）ポストエフェクト
class CopyPostEffect : public IPostEffect {
public:
    void Initialize() override;
    void Apply(ID3D12GraphicsCommandList* commandList) override;

private:
    struct Params {
        float colorScale[3];
        float strength;
    };

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsResource_;
    Params* paramsMapped_ = nullptr;
};

/// @brief グレースケール / セピア共通のカラーフィルターポストエフェクト
class ColorFilterPostEffect : public IPostEffect {
public:
    explicit ColorFilterPostEffect(bool isSepia);

    void Initialize() override;
    void Apply(ID3D12GraphicsCommandList* commandList) override;

    void SetStrength(float strength) { strengthNext_ = strength; }
    float GetStrength() const { return strengthNext_; }

private:
    struct Params {
        float colorScale[3];
        float strength;
    };

    bool isSepia_ = false;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsResource_;
    Params* paramsMapped_ = nullptr;

    float strengthNext_ = 1.0f;
    float currentStrength_ = -1.0f;
};

/// @brief ビネットポストエフェクト
class VignettePostEffect : public IPostEffect {
public:
    void Initialize() override;
    void Apply(ID3D12GraphicsCommandList* commandList) override;

    void SetParams(float scale, float exponent) {
        scaleNext_ = scale;
        exponentNext_ = exponent;
    }
    float GetScale() const { return scaleNext_; }
    float GetExponent() const { return exponentNext_; }

private:
    struct Params {
        float scale;
        float exponent;
    };

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsResource_;
    Params* paramsMapped_ = nullptr;

    float scaleNext_ = 16.0f;
    float exponentNext_ = 0.8f;
    float currentScale_ = -1.0f;
    float currentExponent_ = -1.0f;
};

/// @brief 平滑化（ボックスブラー）ポストエフェクト
class SmoothingPostEffect : public IPostEffect {
public:
    void Initialize() override;
    void Apply(ID3D12GraphicsCommandList* commandList) override;

    void SetParams(int kernelSize) { kernelSizeNext_ = kernelSize; }
    int GetKernelSize() const { return kernelSizeNext_; }

private:
    struct Params {
        int kernelSize;
        float padding[3];
    };

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsResource_;
    Params* paramsMapped_ = nullptr;

    int kernelSizeNext_ = 3;
    int currentKernelSize_ = -1;
};

/// @brief ガウシアンぼかしポストエフェクト
class GaussianBlurPostEffect : public IPostEffect {
public:
    void Initialize() override;
    void Apply(ID3D12GraphicsCommandList* commandList) override;

    void SetParams(int kernelSize, float sigma) {
        kernelSizeNext_ = kernelSize;
        sigmaNext_ = sigma;
    }
    int GetKernelSize() const { return kernelSizeNext_; }
    float GetSigma() const { return sigmaNext_; }

private:
    struct Params {
        int kernelSize;
        float sigma;
        float padding[2];
    };

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsResource_;
    Params* paramsMapped_ = nullptr;

    int kernelSizeNext_ = 3;
    float sigmaNext_ = 2.0f;
    int currentKernelSize_ = -1;
    float currentSigma_ = -1.0f;
};

/// @brief アウトライン（輪郭線）ポストエフェクト
class OutlinePostEffect : public IPostEffect {
public:
    void Initialize() override;
    void Apply(ID3D12GraphicsCommandList* commandList) override;
    bool NeedsDepthResource() const override { return true; }

    void SetParams(float depthEdgeMultiplier, float colorEdgeMultiplier) {
        depthEdgeMultiplierNext_ = depthEdgeMultiplier;
        colorEdgeMultiplierNext_ = colorEdgeMultiplier;
    }
    float GetDepthEdgeMultiplier() const { return depthEdgeMultiplierNext_; }
    float GetColorEdgeMultiplier() const { return colorEdgeMultiplierNext_; }

private:
    struct Params {
        Matrix4x4 projectionInverse;
        float depthEdgeMultiplier;
        float colorEdgeMultiplier;
        float padding[2];
    };

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsResource_;
    Params* paramsMapped_ = nullptr;

    float depthEdgeMultiplierNext_ = 6.0f;
    float colorEdgeMultiplierNext_ = 1.0f;
    Matrix4x4 currentProjectionInverse_{};
    float currentDepthEdgeMultiplier_ = -1.0f;
    float currentColorEdgeMultiplier_ = -1.0f;
};

/// @brief 放射状ブラーポストエフェクト
class RadialBlurPostEffect : public IPostEffect {
public:
    void Initialize() override;
    void Apply(ID3D12GraphicsCommandList* commandList) override;

    void SetParams(float centerX, float centerY, float width, int sampleCount) {
        centerXNext_ = centerX;
        centerYNext_ = centerY;
        widthNext_ = width;
        sampleCountNext_ = sampleCount;
    }
    float GetCenterX() const { return centerXNext_; }
    float GetCenterY() const { return centerYNext_; }
    float GetWidth() const { return widthNext_; }
    int GetSampleCount() const { return sampleCountNext_; }

private:
    struct Params {
        float center[2];
        float blurWidth;
        int sampleCount;
    };

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsResource_;
    Params* paramsMapped_ = nullptr;

    float centerXNext_ = 0.5f;
    float centerYNext_ = 0.5f;
    float widthNext_ = 0.01f;
    int sampleCountNext_ = 10;
    float currentCenterX_ = -1.0f;
    float currentCenterY_ = -1.0f;
    float currentWidth_ = -1.0f;
    int currentSampleCount_ = -1;
};

/// @brief ディゾルブポストエフェクト
class DissolvePostEffect : public IPostEffect {
public:
    void Initialize() override;
    void Apply(ID3D12GraphicsCommandList* commandList) override;

    void SetParams(const float edgeColor[3], float threshold, float edgeRange) {
        edgeColorNext_[0] = edgeColor[0];
        edgeColorNext_[1] = edgeColor[1];
        edgeColorNext_[2] = edgeColor[2];
        thresholdNext_ = threshold;
        edgeRangeNext_ = edgeRange;
    }
    void SetMaskTexture(const std::string& maskPath) { maskPathNext_ = maskPath; }

    float GetThreshold() const { return thresholdNext_; }
    float GetEdgeRange() const { return edgeRangeNext_; }
    const float* GetEdgeColor() const { return edgeColorNext_; }

private:
    struct Params {
        float edgeColor[3];
        float threshold;
        float edgeRange;
        float padding[3];
    };

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsResource_;
    Params* paramsMapped_ = nullptr;

    float edgeColorNext_[3] = { 1.0f, 0.4f, 0.3f };
    float thresholdNext_ = 0.0f;
    float edgeRangeNext_ = 0.02f;
    std::string maskPathNext_ = "masks/noise0.png";

    float currentEdgeColor_[3] = { -1.0f, -1.0f, -1.0f };
    float currentThreshold_ = -1.0f;
    float currentEdgeRange_ = -1.0f;
    std::string currentMaskPath_;
};

/// @brief ランダムノイズポストエフェクト
class RandomPostEffect : public IPostEffect {
public:
    void Initialize() override;
    void Update(float deltaTime) override;
    void Apply(ID3D12GraphicsCommandList* commandList) override;

    void SetParams(float strength) { strengthNext_ = strength; }
    float GetStrength() const { return strengthNext_; }

private:
    struct Params {
        float time;
        float strength;
        float padding[2];
    };

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsResource_;
    Params* paramsMapped_ = nullptr;

    float strengthNext_ = 0.0f;
    float currentTime_ = 0.0f;
};

/// @brief HSV色空間調整ポストエフェクト
class HSVPostEffect : public IPostEffect {
public:
    void Initialize() override;
    void Apply(ID3D12GraphicsCommandList* commandList) override;

    void SetParams(float hue, float saturation, float value) {
        hueNext_ = hue;
        saturationNext_ = saturation;
        valueNext_ = value;
    }
    float GetHue() const { return hueNext_; }
    float GetSaturation() const { return saturationNext_; }
    float GetValue() const { return valueNext_; }

private:
    struct Params {
        float hue;
        float saturation;
        float value;
        float padding;
    };

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsResource_;
    Params* paramsMapped_ = nullptr;

    float hueNext_ = 0.0f;
    float saturationNext_ = 0.0f;
    float valueNext_ = 0.0f;
    float currentHue_ = -2.0f;
    float currentSaturation_ = -2.0f;
    float currentValue_ = -2.0f;
};
