#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>
#include <cassert> // assert用に必要
#include <memory> // std::unique_ptr用に必要

// SRV管理
class SrvManager {
public:
  // 【Passkey Idiom】
  struct Token {
  private:
    friend class SrvManager;
    Token() {}
  };

private: // namespace省略のためのusing宣言
#pragma region using宣言

  // Microsoft::WRL::ComPtrをComPtrで省略
  template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

#pragma endregion

public: // メンバ変数
  // 最大SRV数(最大テクスチャ枚数)
  static const uint32_t kMaxSRVCount;

private: // メンバ変数
  // シングルトンインスタンス
  static std::unique_ptr<SrvManager> instance_;

  // デスクリプタヒープサイズ
  uint32_t descriptorSize_ = 0; // SRV/UAV/CBV用
  // デスクリプタヒープ
  ComPtr<ID3D12DescriptorHeap> descriptorHeap_ = nullptr; // SRV/UAV/CBV用

  // 次に使用するSRVインデックス
  uint32_t useIndex_ = 0;

public: // シングルトンインスタンス取得
    static SrvManager* GetInstance();

public: // メンバ関数
  explicit SrvManager(Token);
  // 初期化
  void Initialize();

  // 終了
  void Finalize();

  // 描画前処理
  void PreDraw();

  // テクスチャ枚数上限チェック
  bool AllocatableTexture();

  uint32_t Allocate();

  // SRVをセット
  void SetGraphicsRootDescriptorTable(UINT RootParameterIndex,
                                     uint32_t srvIndex);

  // Compute用にSRV/UAVをセット
  void SetComputeRootDescriptorTable(UINT RootParameterIndex,
                                     uint32_t descriptorIndex);

  // コンピュート前処理（デスクリプタヒープの設定）
  void PreCompute();

  /// <summary>
  /// デスクリプタヒープのゲッター
  /// </summary>
  /// <returns>デスクリプタヒープのポインタ</returns>
  ID3D12DescriptorHeap *GetDescriptorHeap() const {
    return descriptorHeap_.Get();
  }

  /// <summary>
  /// SRVの指定したインデックスのCPUディスクリプタハンドルを取得
  /// </summary>
  /// <param name="index">取得するSRVのインデックス</param>
  D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

  /// <summary>
  /// SRVの指定したインデックスのGPUディスクリプタハンドルを取得
  /// </summary>
  /// <param name="index">取得するSRVのインデックス</param>
  D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

  /// <summary>
  /// テクスチャ用のSRVを作成
  /// </summary>
  /// <param name="srvIndex">デスクリプタヒープ上のインデックス</param>
  /// <param name="resource">StructuredBufferのID3D12Resource</param>
  /// <param name="Format">テクスチャのフォーマット</param>
  /// <param name="MipLevels">ミップマップレベル数</param>
  /// <param name="isCubeMap">CubeMapかどうか</param>
  void CreateSRVForTexture(uint32_t srvIndex, ComPtr<ID3D12Resource> resource,
                           DXGI_FORMAT Format, UINT MipLevels, bool isCubeMap);

  /// <summary>
  /// StructuredBuffer用のSRVを作成
  /// </summary>
  /// <param name="srvIndex">デスクリプタヒープ上のインデックス</param>
  /// <param name="resource">StructuredBufferのID3D12Resource</param>
  /// <param name="numElement">格納されている要素（インスタンス）の数</param>
  /// <param name="structureByteStride">構造体（要素）のバイトサイズ</param>
  void CreateSRVForStructuredBuffer(uint32_t srvIndex,
                                    ComPtr<ID3D12Resource> resource,
                                    uint32_t numElement,
                                    uint32_t structureByteStride);

  /// <summary>
  /// StructuredBuffer用のUAVを作成
  /// </summary>
  /// <param name="uavIndex">デスクリプタヒープ上のインデックス</param>
  /// <param name="resource">StructuredBufferのID3D12Resource</param>
  /// <param name="numElement">格納されている要素の数</param>
  /// <param name="structureByteStride">構造体（要素）のバイトサイズ</param>
  void CreateUAVForStructuredBuffer(uint32_t uavIndex,
                                    ComPtr<ID3D12Resource> resource,
                                    uint32_t numElement,
                                    uint32_t structureByteStride);

private: // コンストラクタ周りをprivateへ
    ~SrvManager() = default;
    SrvManager(const SrvManager&) = delete;
    const SrvManager& operator=(const SrvManager&) = delete;

    friend std::default_delete<SrvManager>;
};
