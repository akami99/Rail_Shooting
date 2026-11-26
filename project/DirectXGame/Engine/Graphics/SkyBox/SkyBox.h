#pragma once
#include "Types/GraphicsTypes.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

class Camera;

// スカイボックス用定数バッファデータ
struct SkyboxTransformData {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Vector2 uvScale;
    float padding[2];
};

class Skybox {
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;
public:
    static const uint32_t kMaxViews = 3; // 0: Main, 1: Left, 2: Right

    // スカイボックスの描画モード
    enum class SkyboxMode {
        CubeMap, // 通常のキューブマップ (skybox.dds等)
        Tile     // 2Dタイル画像のリピート描画 (tile.png等)
    };

private:
    // 頂点・インデックスバッファ
    ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    ComPtr<ID3D12Resource> indexResource_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    // 定数バッファ (WVP行列, UVスケール)
    ComPtr<ID3D12Resource> transformResources_[kMaxViews];
    SkyboxTransformData* transformData_[kMaxViews] = {nullptr};
    // PSO (CubeMap用 / Tile用)
    ComPtr<ID3D12PipelineState> psoCubeMap_;
    ComPtr<ID3D12PipelineState> psoTile_;
    // 現在のモード
    SkyboxMode mode_ = SkyboxMode::CubeMap;
    // テクスチャのSRVインデックス
    uint32_t textureSrvIndex_ = 0;
    std::string texturePath_ = "skybox.dds";
    Vector2 uvScale_ = { 20.0f, 20.0f };
    // カメラ参照
    Camera* camera_ = nullptr;
    // トランスフォーム (Debug/ImGui用)
    Vector3 scale_ = { 500.0f, 500.0f, 500.0f };
    Vector3 rotate_ = { 0.0f, 0.0f, 0.0f };
    Vector3 translate_ = { 0.0f, 0.0f, 0.0f };
public:
	~Skybox();
    // 通常のキューブマップ用初期化
    void Initialize(const std::string& cubeMapPath = "skybox.dds");
    // タイリング用初期化
    void InitializeTile(const std::string& texturePath = "tile.png", const Vector2& uvScale = { 20.0f, 20.0f });

	// 全ビュー更新
    void Update();
	// 指定したビュー用の更新
    void Update(uint32_t viewIndex, Camera* camera);
    void Draw(uint32_t viewIndex = 0);
    void SetCamera(Camera* camera) { camera_ = camera; }

    // モード設定・取得
    void SetMode(SkyboxMode mode) { mode_ = mode; }
    SkyboxMode GetMode() const { return mode_; }

    // テクスチャ設定
    void SetTexture(const std::string& texturePath);
    void SetCubeMap(const std::string& cubeMapPath);
    void SetTileTexture(const std::string& texturePath);
    const std::string& GetTexturePath() const { return texturePath_; }

    // UVスケール（タイリング調整）
    const Vector2& GetUvScale() const { return uvScale_; }
    void SetUvScale(const Vector2& uvScale) { uvScale_ = uvScale; }

    // ゲッター・セッター
    const Vector3& GetScale() const { return scale_; }
    void SetScale(const Vector3& scale) { scale_ = scale; }
    const Vector3& GetRotate() const { return rotate_; }
    void SetRotate(const Vector3& rotate) { rotate_ = rotate; }
    const Vector3& GetTranslate() const { return translate_; }
    void SetTranslate(const Vector3& translate) { translate_ = translate; }
private:
    void CreateVertexResource();   // 箱の頂点データ生成
    void CreateIndexResource();    // 箱のインデックスデータ生成
	void CreateTransformResource();// 定数バッファ生成
};