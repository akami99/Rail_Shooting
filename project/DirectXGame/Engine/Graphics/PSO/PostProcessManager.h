#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <string>
#include <algorithm>
#include "Types/GraphicsTypes.h"

class RenderTexture;

/// @brief ポストプロセス（グレースケール・セピア等）の PSO 切り替えと
///        定数バッファ更新をカプセル化するシングルトン管理クラス
class PostProcessManager {
public:
#pragma region ポストエフェクトタイプ
    // --- ポストエフェクトモード定数 ---
    static constexpr int kModeCopy      = 0; // パススルー（無効果）
    static constexpr int kModeGrayscale = 1;
    static constexpr int kModeSepia     = 2;
    static constexpr int kModeVignette  = 3;
    static constexpr int kModeSmoothing = 4;
    static constexpr int kModeGaussianBlur = 5;
    static constexpr int kModeOutline = 6;
    static constexpr int kModeRadialBlur = 7;
    static constexpr int kModeDissolve = 8;
    static constexpr int kModeRandom = 9;
    static constexpr int kModeHSV = 10;
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
    template <class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    // グレースケール/セピア共通の定数バッファ構造体
    struct PostProcessParams {
        float colorScale[3]; // gColorScale に対応 (float3)
        float strength;      // 0.0f: 元の色, 1.0f: フィルター色
    };

    // ビネット用の定数バッファ構造体
    struct VignetteParams {
        float scale;
        float exponent;
    };

    // 平滑化用の定数バッファ構造体
    struct SmoothingParams {
        int kernelSize;
        float padding[3]; // 16バイトアライメント
    };

    // ガウシアンフィルター用の定数バッファ構造体
    struct GaussianBlurParams {
        int kernelSize;
        float sigma;
        float padding[2]; // 16バイトアライメント
    };

    // アウトライン用の定数バッファ構造体
    struct OutlineParams {
        Matrix4x4 projectionInverse;
        float depthEdgeMultiplier;
        float colorEdgeMultiplier;
        float padding[2]; // 16バイトアライメント
    };

    // Radial Blur用の定数バッファ構造体
    struct RadialBlurParams {
        float center[2];
        float blurWidth;
        int sampleCount;
    };

    // Dissolve用の定数バッファ構造体
    struct DissolveParams {
        float edgeColor[3];
        float threshold;
        float edgeRange;
        float padding[3];
    };

    // Random用の定数バッファ構造体
    struct RandomParams {
        float time;
        float strength;
        float padding[2]; // 16バイトアライメント
    };

    // HSV用の定数バッファ構造体
    struct HSVParams {
        float hue;
        float saturation;
        float value;
        float padding; // 16バイトアライメント
    };

    ComPtr<ID3D12PipelineState> postProcessPSO_; // CopyImage (passthrough)
    ComPtr<ID3D12PipelineState> colorFilterPSO_; // グレースケール/セピア
    ComPtr<ID3D12PipelineState> vignettePSO_;    // ビネット
    ComPtr<ID3D12PipelineState> smoothingPSO_;   // 平滑化
    ComPtr<ID3D12PipelineState> gaussianBlurPSO_; // ガウシアンフィルター
    ComPtr<ID3D12PipelineState> outlinePSO_;      // アウトライン
    ComPtr<ID3D12PipelineState> radialBlurPSO_;   // Radial Blur
    ComPtr<ID3D12PipelineState> dissolvePSO_;     // Dissolve
    ComPtr<ID3D12Resource> paramsResource_;
    PostProcessParams* paramsMapped_ = nullptr;

    ComPtr<ID3D12Resource> vignetteParamsResource_;
    VignetteParams* vignetteParamsMapped_ = nullptr;

    ComPtr<ID3D12Resource> smoothingParamsResource_;
    SmoothingParams* smoothingParamsMapped_ = nullptr;

    ComPtr<ID3D12Resource> gaussianBlurParamsResource_;
    GaussianBlurParams* gaussianBlurParamsMapped_ = nullptr;

    ComPtr<ID3D12Resource> outlineParamsResource_;
    OutlineParams* outlineParamsMapped_ = nullptr;

    ComPtr<ID3D12Resource> radialBlurParamsResource_;
    RadialBlurParams* radialBlurParamsMapped_ = nullptr;

    ComPtr<ID3D12Resource> dissolveParamsResource_;
    DissolveParams* dissolveParamsMapped_ = nullptr;
    
    ComPtr<ID3D12PipelineState> randomPSO_;
    ComPtr<ID3D12Resource> randomParamsResource_;
    RandomParams* randomParamsMapped_ = nullptr;

    ComPtr<ID3D12PipelineState> hsvPSO_;
    ComPtr<ID3D12Resource> hsvParamsResource_;
    HSVParams* hsvParamsMapped_ = nullptr;

    uint32_t depthSrvIndex_ = 0;

    // 遅延適用用 (Update から Draw への橋渡し)
    int modeNext_ = 0;
    float strengthNext_ = 1.0f;
    float vignetteScaleNext_ = 16.0f;
    float vignetteExponentNext_ = 0.8f;
    int smoothingKernelSizeNext_ = 3;
    int gaussianBlurKernelSizeNext_ = 3;
    float gaussianBlurSigmaNext_ = 2.0f;
    float outlineDepthEdgeMultiplierNext_ = 6.0f;
    float outlineColorEdgeMultiplierNext_ = 1.0f;
    float radialBlurCenterXNext_ = 0.5f;
    float radialBlurCenterYNext_ = 0.5f;
    float radialBlurWidthNext_ = 0.01f;
    int radialBlurSampleCountNext_ = 10;
    float dissolveEdgeColorNext_[3] = { 1.0f, 0.4f, 0.3f };
    float dissolveThresholdNext_ = 0.0f;
    float dissolveEdgeRangeNext_ = 0.02f;
    std::string dissolveMaskPathNext_ = "masks/noise0.png";
    float randomStrengthNext_ = 0.0f;
    float hsvHueNext_ = 0.0f;
    float hsvSaturationNext_ = 0.0f;
    float hsvValueNext_ = 0.0f;
    int currentMode_ = kModeCopy;

    // パラメータ適用最適化用（変更時のみ定数バッファを更新するため）
    float currentStrength_ = -1.0f;
    int currentColorFilterMode_ = -1;
    float currentVignetteScale_ = -1.0f;
    float currentVignetteExponent_ = -1.0f;
    int currentSmoothingKernelSize_ = -1;
    int currentGaussianBlurKernelSize_ = -1;
    float currentGaussianBlurSigma_ = -1.0f;
    Matrix4x4 currentProjectionInverse_{};
    float currentOutlineDepthEdgeMultiplier_ = -1.0f;
    float currentOutlineColorEdgeMultiplier_ = -1.0f;
    float currentRadialBlurCenterX_ = -1.0f;
    float currentRadialBlurCenterY_ = -1.0f;
    float currentRadialBlurWidth_ = -1.0f;
    int currentRadialBlurSampleCount_ = -1;
    float currentDissolveEdgeColor_[3];
    float currentDissolveThreshold_ = -1.0f;
    float currentDissolveEdgeRange_ = -1.0f;
    std::string currentDissolveMaskPath_;
    float currentRandomStrength_ = -1.0f;
    float currentRandomTime_ = 0.0f;
    float currentHSVHue_ = -2.0f;
    float currentHSVSaturation_ = -2.0f;
    float currentHSVValue_ = -2.0f;

    static std::unique_ptr<PostProcessManager> instance_;

public:
    static PostProcessManager* GetInstance();
    static void Destroy();

    explicit PostProcessManager(Token) {}

    /// <summary>
    /// PSO・定数バッファを初期化する
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

    /// <summary>
    /// ポストエフェクトモードを設定する
    /// </summary>
    /// <param name="mode">ポストエフェクトモード</param>
    void SetMode(int mode)         { modeNext_ = mode; }

    /// <summary>
    /// カラーフィルタの強さを設定する
    /// </summary>
    /// <param name="s">強さ</param>
    void SetStrength(float s)      { strengthNext_ = s; }

    /// <summary>
    /// ビネット効果のパラメータを設定する
    /// </summary>
    /// <param name="scale">スケール</param>
    /// <param name="exponent">指数</param>
    void SetVignetteParams(float scale, float exponent) {
        vignetteScaleNext_ = std::clamp(scale, kMinVignetteScale, kMaxVignetteScale);
        vignetteExponentNext_ = std::clamp(exponent, kMinVignetteExponent, kMaxVignetteExponent);
    }

    /// <summary>
    /// 平滑化のパラメータを設定する（偶数の場合は奇数に自動補正されます）
    /// </summary>
    /// <param name="kernelSize">カーネルサイズ</param>
    void SetSmoothingParams(int kernelSize) {
        int clampedSize = std::clamp(kernelSize, kMinSmoothingKernelSize, kMaxSmoothingKernelSize);
        if (clampedSize % 2 == 0) {
            clampedSize += 1;
        }
        smoothingKernelSizeNext_ = clampedSize;
    }

    /// <summary>
    /// ガウシアンぼかしのパラメータを設定する（カーネルサイズが偶数の場合は奇数に自動補正されます）
    /// </summary>
    /// <param name="kernelSize">カーネルサイズ</param>
    /// <param name="sigma">シグマ</param>
    void SetGaussianBlurParams(int kernelSize, float sigma) {
        int clampedSize = std::clamp(kernelSize, kMinGaussianBlurKernelSize, kMaxGaussianBlurKernelSize);
        if (clampedSize % 2 == 0) {
            clampedSize += 1;
        }
        gaussianBlurKernelSizeNext_ = clampedSize;
        gaussianBlurSigmaNext_ = std::clamp(sigma, kMinGaussianBlurSigma, kMaxGaussianBlurSigma);
    }

    /// <summary>
    /// 輪郭線のパラメータを設定する
    /// </summary>
    /// <param name="depthEdgeMultiplier">深度エッジの倍数</param>
    /// <param name="colorEdgeMultiplier">色エッジの倍数</param>
    void SetOutlineParams(float depthEdgeMultiplier, float colorEdgeMultiplier) {
        outlineDepthEdgeMultiplierNext_ = std::clamp(depthEdgeMultiplier, kMinOutlineDepthEdgeMultiplier, kMaxOutlineDepthEdgeMultiplier);
        outlineColorEdgeMultiplierNext_ = std::clamp(colorEdgeMultiplier, kMinOutlineColorEdgeMultiplier, kMaxOutlineColorEdgeMultiplier);
    }

    /// <summary>
    /// 放射状ぼかしのパラメータを設定する
    /// </summary>
    /// <param name="centerX">ぼかしの中心X座標</param>
    /// <param name="centerY">ぼかしの中心Y座標</param>
    /// <param name="width">ぼかしの幅</param>
    /// <param name="sampleCount">サンプル数</param>
    void SetRadialBlurParams(float centerX, float centerY, float width, int sampleCount) {
        radialBlurCenterXNext_ = std::clamp(centerX, kMinRadialBlurCenterX, kMaxRadialBlurCenterX);
        radialBlurCenterYNext_ = std::clamp(centerY, kMinRadialBlurCenterY, kMaxRadialBlurCenterY);
        radialBlurWidthNext_ = std::clamp(width, kMinRadialBlurWidth, kMaxRadialBlurWidth);
        radialBlurSampleCountNext_ = std::clamp(sampleCount, kMinRadialBlurSampleCount, kMaxRadialBlurSampleCount);
    }

    /// <summary>
    /// 解体効果のパラメータを設定する
    /// </summary>
    /// <param name="edgeColor">エッジの色</param>
    /// <param name="threshold">閾値</param>
    /// <param name="edgeRange">エッジの範囲</param>
    void SetDissolveParams(const float edgeColor[3], float threshold, float edgeRange) {
        dissolveEdgeColorNext_[0] = std::clamp(edgeColor[0], 0.0f, 1.0f);
        dissolveEdgeColorNext_[1] = std::clamp(edgeColor[1], 0.0f, 1.0f);
        dissolveEdgeColorNext_[2] = std::clamp(edgeColor[2], 0.0f, 1.0f);
        dissolveThresholdNext_ = std::clamp(threshold, kMinDissolveThreshold, kMaxDissolveThreshold);
        dissolveEdgeRangeNext_ = std::clamp(edgeRange, kMinDissolveEdgeRange, kMaxDissolveEdgeRange);
    }

    /// <summary>
    /// 解体効果のマスクテクスチャを設定する
    /// </summary>
    /// <param name="maskPath">マスクテクスチャのパス</param>
    void SetDissolveMaskTexture(const std::string& maskPath) {
        dissolveMaskPathNext_ = maskPath;
    }

    /// <summary>
    /// ランダム効果のパラメータを設定する
    /// </summary>
    /// <param name="strength">強さ</param>
    void SetRandomParams(float strength) {
        randomStrengthNext_ = std::clamp(strength, kMinRandomStrength, kMaxRandomStrength);
    }

    /// <summary>
    /// HSV効果のパラメータを設定する
    /// </summary>
    /// <param name="hue">色相</param>
    /// <param name="saturation">彩度</param>
    /// <param name="value">明度</param>
    void SetHSVParams(float hue, float saturation, float value) {
        hsvHueNext_ = std::clamp(hue, kMinHSVHue, kMaxHSVHue);
        hsvSaturationNext_ = std::clamp(saturation, kMinHSVSaturation, kMaxHSVSaturation);
        hsvValueNext_ = std::clamp(value, kMinHSVValue, kMaxHSVValue);
    }

    // --- パラメータ取得用ゲッター (双方向同期用) ---
    float GetVignetteScale() const { return vignetteScaleNext_; }
    float GetVignetteExponent() const { return vignetteExponentNext_; }
    int GetSmoothingKernelSize() const { return smoothingKernelSizeNext_; }
    int GetGaussianBlurKernelSize() const { return gaussianBlurKernelSizeNext_; }
    float GetGaussianBlurSigma() const { return gaussianBlurSigmaNext_; }
    float GetOutlineDepthEdgeMultiplier() const { return outlineDepthEdgeMultiplierNext_; }
    float GetOutlineColorEdgeMultiplier() const { return outlineColorEdgeMultiplierNext_; }
    float GetRadialBlurCenterX() const { return radialBlurCenterXNext_; }
    float GetRadialBlurCenterY() const { return radialBlurCenterYNext_; }
    float GetRadialBlurWidth() const { return radialBlurWidthNext_; }
    int GetRadialBlurSampleCount() const { return radialBlurSampleCountNext_; }
    float GetDissolveThreshold() const { return dissolveThresholdNext_; }
    float GetDissolveEdgeRange() const { return dissolveEdgeRangeNext_; }
    const float* GetDissolveEdgeColor() const { return dissolveEdgeColorNext_; }
    float GetRandomStrength() const { return randomStrengthNext_; }
    float GetHSVHue() const { return hsvHueNext_; }
    float GetHSVSaturation() const { return hsvSaturationNext_; }
    float GetHSVValue() const { return hsvValueNext_; }

    /// <summary>
    /// 現在のポストエフェクトモードを取得する
    /// </summary>
    /// <returns></returns>
    int GetCurrentMode() const            { return currentMode_; }

private:
    void DrawCopy(ID3D12GraphicsCommandList* commandList);
    void DrawVignette(ID3D12GraphicsCommandList* commandList, float scale, float exponent);
    void DrawSmoothing(ID3D12GraphicsCommandList* commandList, int kernelSize);
    void DrawGaussianBlur(ID3D12GraphicsCommandList* commandList, int kernelSize, float sigma);
    void DrawOutline(ID3D12GraphicsCommandList* commandList, float depthEdgeMultiplier, float colorEdgeMultiplier);
    void DrawRadialBlur(ID3D12GraphicsCommandList* commandList, float centerX, float centerY, float width, int sampleCount);
    void DrawDissolve(ID3D12GraphicsCommandList* commandList, float threshold, float edgeRange, const float edgeColor[3], const std::string& maskPath);
    void DrawRandom(ID3D12GraphicsCommandList* commandList, float strength);
    void DrawColorFilter(ID3D12GraphicsCommandList* commandList, float strength);
    void DrawHSV(ID3D12GraphicsCommandList* commandList, float hue, float saturation, float value);

    ~PostProcessManager() = default;
    PostProcessManager(const PostProcessManager&) = delete;
    PostProcessManager& operator=(const PostProcessManager&) = delete;
    friend std::default_delete<PostProcessManager>;
};
