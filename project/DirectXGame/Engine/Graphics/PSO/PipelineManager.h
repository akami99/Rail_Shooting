#pragma once

#include <d3d12.h>
#include <dxcapi.h> // IDxcBlob用に必要
#include <wrl/client.h>
#include <cstdint> // uint32_t用に必要
#include <memory> // std::unique_ptr用に必要

class PipelineManager {
public:
    // 【Passkey Idiom】
    struct Token {
    private:
        friend class PipelineManager;
        Token() {}
    };

private: // namespace省略のためのusing宣言
#pragma region using宣言

    // Microsoft::WRL::ComPtrをComPtrで省略
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

#pragma endregion

private: // メンバ変数
    // --- Sprite描画用アセット ---
    ComPtr<IDxcBlob> vsBlobSprite_;                   // Vertex Shader
    ComPtr<IDxcBlob> psBlobSprite_;                   // Pixel Shader
    ComPtr<ID3D12RootSignature> rootSignatureSprite_; // ルートシグネチャ

    // --- 3D描画用アセット ---
    ComPtr<IDxcBlob> vsBlob3D_;                   // Vertex Shader
    ComPtr<IDxcBlob> psBlob3D_;                   // Pixel Shader
    ComPtr<ID3D12RootSignature> rootSignature3D_; // ルートシグネチャ

    // --- Particle描画用アセット ---
    ComPtr<IDxcBlob> vsBlobParticle_;                   // Vertex Shader
    ComPtr<IDxcBlob> psBlobParticle_;                   // Pixel Shader
    ComPtr<ID3D12RootSignature> rootSignatureParticle_; // ルートシグネチャ

    // --- Skybox描画用アセット ---
    ComPtr<IDxcBlob> vsBlobSkybox_;                   // Vertex Shader
    ComPtr<IDxcBlob> psBlobSkybox_;                   // Pixel Shader
    ComPtr<ID3D12RootSignature> rootSignatureSkybox_; // ルートシグネチャ

    // --- ポストエフェクト用アセット (CopyImage passthrough) ---
    ComPtr<IDxcBlob> vsBlobPostProcess_;                   // Vertex Shader
    ComPtr<IDxcBlob> psBlobPostProcess_;                   // Pixel Shader
    ComPtr<ID3D12RootSignature> rootSignaturePostProcess_; // ルートシグネチャ

    // --- カラーフィルター用アセット (グレースケール/セピア) ---
    ComPtr<IDxcBlob> vsBlobColorFilter_; // Vertex Shader (Fullscreenと共用)
    ComPtr<IDxcBlob> psBlobColorFilter_; // Pixel Shader (Grayscale.PS.hlsl)

    // --- ビネット用アセット ---
    ComPtr<IDxcBlob> psBlobVignette_;    // Pixel Shader (Vignette.PS.hlsl)

    // --- 平滑化用アセット ---
    ComPtr<IDxcBlob> psBlobSmoothing_;   // Pixel Shader (Smoothing.PS.hlsl)

    // --- Gaussian Blur用アセット ---
    ComPtr<IDxcBlob> psBlobGaussianBlur_; // Pixel Shader (GaussianBlur.PS.hlsl)

    // --- アウトライン用アセット ---
    ComPtr<IDxcBlob> psBlobOutline_;      // Pixel Shader (Outline.PS.hlsl)

    // --- Radial Blur用アセット ---
    ComPtr<IDxcBlob> psBlobRadialBlur_;   // Pixel Shader (RadialBlur.PS.hlsl)

    // --- Dissolve用アセット ---
    ComPtr<IDxcBlob> psBlobDissolve_;     // Pixel Shader (Dissolve.PS.hlsl)

    // --- Random用アセット ---
    ComPtr<IDxcBlob> psBlobRandom_;       // Pixel Shader (Random.PS.hlsl)

    // --- HSV用アセット ---
    ComPtr<IDxcBlob> psBlobHSV_;          // Pixel Shader (HSV.PS.hlsl)

private: // シングルトン管理用メンバ変数
    static std::unique_ptr<PipelineManager> instance_;

public: // シングルトンインスタンスの取得・破棄
    static PipelineManager* GetInstance();
    static void Destroy();

public: // メンバ関数
    // コンストラクタ(隠蔽)
    explicit PipelineManager(Token);
    // 初期化(シェーダーとルートシグネチャのロード/生成を実行)
    void Initialize();

    // Sprite用のグラフィックスパイプラインを生成して返す関数
    ComPtr<ID3D12PipelineState>
        CreateSpritePSO(const D3D12_BLEND_DESC& blendDesc);

    // Particle用のグラフィックスパイプラインを生成して返す関数
    ComPtr<ID3D12PipelineState>
        CreateParticlePSO(const D3D12_BLEND_DESC& blendDesc);

    // Skybox用のグラフィックスパイプラインを生成して返す関数
    ComPtr<ID3D12PipelineState> CreateSkyboxPSO();

    // 3D用のグラフィックスパイプラインを生成して返す関数
    ComPtr<ID3D12PipelineState> CreateObject3dPSO(
        const D3D12_INPUT_ELEMENT_DESC* inputLayout, // Object3dCommonが決定
        uint32_t numElements,
        const D3D12_RASTERIZER_DESC& rasterizerDesc, // Object3dCommonが決定
        const D3D12_DEPTH_STENCIL_DESC& depthStencilDesc,
        const D3D12_BLEND_DESC& blendDesc);

    // ポストエフェクト用のグラフィックスパイプラインを生成して返す関数 (CopyImage passthrough)
    ComPtr<ID3D12PipelineState> CreatePostProcessPSO();

    // グレースケール/セピアフィルター用 PSOを生成して返す関数
    ComPtr<ID3D12PipelineState> CreateColorFilterPSO();

    // ビネット用 PSOを生成して返す関数
    ComPtr<ID3D12PipelineState> CreateVignettePSO();

    // 平滑化用 PSOを生成して返す関数
    ComPtr<ID3D12PipelineState> CreateSmoothingPSO();

    // Gaussian Blur用 PSOを生成して返す関数
    ComPtr<ID3D12PipelineState> CreateGaussianBlurPSO();

    // アウトライン用 PSOを生成して返す関数
    ComPtr<ID3D12PipelineState> CreateOutlinePSO();

    // Radial Blur用 PSOを生成して返す関数
    ComPtr<ID3D12PipelineState> CreateRadialBlurPSO();

    // Dissolve用 PSOを生成して返す関数
    ComPtr<ID3D12PipelineState> CreateDissolvePSO();

    // Random用 PSOを生成して返す関数
    ComPtr<ID3D12PipelineState> CreateRandomPSO();

    // HSV用 PSOを生成して返す関数
    ComPtr<ID3D12PipelineState> CreateHSVPSO();

    // Compute PSOを生成して返す関数
    ComPtr<ID3D12PipelineState> CreateComputePSO(
        ID3D12RootSignature* rootSignature,
        IDxcBlob* computeShaderBlob);

    // Compute用RootSignatureを生成して返す関数
    ComPtr<ID3D12RootSignature> CreateComputeRootSignature(
        const D3D12_ROOT_SIGNATURE_DESC& desc);

    // ゲッター

    // SpriteCommon用
    ID3D12RootSignature* GetSpriteRootSignature() const {
        return rootSignatureSprite_.Get();
    }

    // Object3dCommon用
    ID3D12RootSignature* Get3DRootSignature() const {
        return rootSignature3D_.Get();
    }

    // ParticleCommon用
    ID3D12RootSignature* GetParticleRootSignature() const {
        return rootSignatureParticle_.Get();
    }

    // Skybox用
    ID3D12RootSignature* GetSkyboxRootSignature() const {
        return rootSignatureSkybox_.Get();
    }

    // ポストエフェクト用
    ID3D12RootSignature* GetPostProcessRootSignature() const {
        return rootSignaturePostProcess_.Get();
    }

private: // メンバ関数
    // シェーダーの読み込み
    void LoadShader();
    // ルートシグネチャの作成
    void CreateRootSignature();

private: // コンストラクタ関連（隠蔽）
    ~PipelineManager() = default;
    PipelineManager(const PipelineManager&) = delete;
    const PipelineManager& operator=(const PipelineManager&) = delete;

    friend std::default_delete<PipelineManager>;
};
