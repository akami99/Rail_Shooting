#pragma once

#include "BlendMode/BlendMode.h"
#include "Types/ModelTypes.h"
#include "Types/ParticleTypes.h"
#include "ParticleEmitter.h"

#include <d3d12.h>
#include <random>
#include <string>
#include <unordered_map>
#include <wrl/client.h>

class Camera;
class Model;

class ParticleManager {
public:
    // 【Passkey Idiom】
    struct Token {
    private:
        friend class ParticleManager;
        Token() {}
    };

private: // namespace省略のためのusing宣言
#pragma region using宣言

    // Microsoft::WRL::ComPtrをComPtrで省略
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

#pragma endregion

private: // メンバ変数
    struct ParticleGroup {
        std::string name;
        std::list<Particle> particles;           // パーティクルのリスト
        ParticleEmitter emitter;

        // マテリアルや定数バッファ関連
        Model * model = nullptr;           // 現在描画に使用するモデルへのポインタ
        Model * defaultModel = nullptr;    // 外部から渡されたデフォルトモデル (null = 矩形/Billboard)
        MaterialData materialData;               // マテリアルデータ
        ComPtr<ID3D12Resource> materialResource; // CBV用リソース
        Material* materialMappedData = nullptr;  // マッピングされたポインタ
        D3D12_GPU_DESCRIPTOR_HANDLE
            instanceSrvHandleGPU; // インスタンシングデータ用SRVインデックス
        ComPtr<ID3D12Resource>
            instanceResource;   // インスタンシングリソース (StructuredBuffer)
        uint32_t instanceCount = 0; // インスタンス数 (初期値0)
        ParticleInstanceData* mappedData =
            nullptr; // インスタンシングデータを書き込むためのポインタ
    };
    std::unordered_map<std::string, ParticleGroup> particleGroups_{};

    // シングルトンインスタンス
    static std::unique_ptr<ParticleManager> instance_;

    // パイプライン・頂点リソース関連のメンバ変数
    ComPtr<ID3D12RootSignature> particleRootSignature_{};
    ComPtr<ID3D12PipelineState>
        particlePsoArray_[BlendMode::BlendState::kCountOfBlendMode]{};
    ComPtr<ID3D12Resource> vertexResource_{};
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    VertexData* vertexData_ = nullptr;

    ComPtr<ID3D12Resource> indexResource_{};    // インデックスバッファリソース
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{}; // インデックスバッファビュー
    uint32_t numIndices_ = 6;                   // 描画インデックス数 (6固定)

    // ランダムエンジン
    std::mt19937 randomEngine_;

    // 状態管理
    bool isUpdate_ = true;
    bool useBillboard_ = true;

    // Particleの最大数
    static const uint32_t kNumMaxParticle = 100;

    // 初期化中に使用したアップロードリソースを保持するリスト
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResources_;

public: // シングルトンインスタンス取得
    static ParticleManager* GetInstance();
    static void Destroy();

public: // メンバ関数
    // コンストラクタ(隠蔽)
    explicit ParticleManager(Token);
    // 初期化処理
    void Initialize();

    // パーティクルグループの生成
    void CreateParticleGroup(const std::string& name,
        const std::string& textureFilePath,
        Model* model = nullptr);

    // パーティクルの発生 (Emit)
    void Emit(const std::string& name, const Vector3& translate, uint32_t count);

    // カスタムパーティクルの発生 (位置・回転・スケール・色・寿命を直接指定)
    void EmitCustom(const std::string& name, const Vector3& translate, const Vector3& rotate, const Vector3& scale, const Vector4& color, float lifeTime);

    // パーティクル生成関数
    Particle MakeNewParticle(const Vector3& translate, const ParticleGenerateSettings& settings);

    // 更新処理
    void Update(const Camera& camera, float deltaTime);

    // 状態設定
    void SetIsUpdate(bool isUpdate) { isUpdate_ = isUpdate; }
    void SetUseBillboard(bool useBillboard) { useBillboard_ = useBillboard; }
    void SetSeed(uint32_t seed) { randomEngine_.seed(seed); }

    // 描画処理
    void Draw(BlendMode::BlendState blendMode);

    void SetEmitter(const std::string& name, const ParticleEmitter& emitter);

    // 全パーティクルグループ名の取得
    std::vector<std::string> GetParticleGroupNames() const;

    // エミッターの取得(ImGui等での編集・参照用)
    ParticleEmitter* GetEmitter(const std::string& name);
    const ParticleEmitter* GetEmitter(const std::string& name) const;

    // パーティクルグループのテクスチャ設定/取得
    void SetGroupTexture(const std::string& name, const std::string& textureFilePath);
    std::string GetGroupTexture(const std::string& name) const;

    // パーティクルのクリア処理
    void ClearParticles(const std::string& name);
    void ClearAllParticles();

    // 解放処理
    void ReleaseIntermediateResources();

    // 終了処理
    void Finalize();

private: // メンバ関数
    // デストラクタ(隠蔽)
    ~ParticleManager() = default;
    // コピーコンストラクタの封印
    ParticleManager(const ParticleManager&) = delete;
    // コピー代入演算子の封印
    ParticleManager& operator=(const ParticleManager&) = delete;

    friend std::default_delete<ParticleManager>;

    // Update 分割ヘルパー
    void UpdateGroupEmitter(ParticleGroup& group, const std::string& name, float deltaTime);
    void UpdateGroupMaterial(ParticleGroup& group, float deltaTime);
    bool UpdateParticle(Particle& particle, const ParticleGroup& group, float deltaTime);
    void WriteInstanceData(ParticleGroup& group, const Camera& camera, const Matrix4x4& viewProjectionMatrix, uint32_t& instanceIndex);
};
