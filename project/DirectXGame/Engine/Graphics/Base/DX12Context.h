#pragma once

#include <array>
#include <chrono>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include "externals/DirectXTex/DirectXTex.h"

// DirectX基盤
class DX12Context {
public: // namespace省略のためのusing宣言
#pragma region using宣言

  // Microsoft::WRL::ComPtrをComPtrで省略
  template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

#pragma endregion

public:
  // 【Passkey Idiom】
  struct Token {
  private:
    friend class DX12Context;
    Token() {}
  };

private: // メンバ変数
#pragma region privateメンバ変数
   
  static std::unique_ptr<DX12Context> instance_; // シングルトンインスタンス

  // DirectX12デバイス
  ComPtr<ID3D12Device> device_ = nullptr;
  // DXGIファクトリ
  ComPtr<IDXGIFactory7> dxgiFactory_ = nullptr;

  // コマンドアロケータ
  ComPtr<ID3D12CommandAllocator> commandAllocator_ = nullptr;
  // コマンドリスト
  ComPtr<ID3D12GraphicsCommandList> commandList_ = nullptr;
  // コマンドキュー
  ComPtr<ID3D12CommandQueue> commandQueue_ = nullptr;

  // スワップチェーン
  ComPtr<IDXGISwapChain4> swapChain_ = nullptr;

  // 深度バッファ
  ComPtr<ID3D12Resource> depthStencilResource_ =
      nullptr; // = CreateDepthStencilTextureResource(device,
               // Win32Window::kClientWidth, Win32Window::kClientHeight);
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_ = {};

  // 各種デスクリプタヒープサイズ
  uint32_t descriptorSizeRTV_ = 0; // RTV用
  uint32_t descriptorSizeDSV_ = 0; // DSV用
  // 各種デスクリプタヒープ
  ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr; // RTV用
  ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr; // DSV用

  // レンダーターゲットビュー
  static const uint32_t kSwapChainResourcesCount =
      2; // スワップチェーンの要素数(ダブルバッファ)
  std::array<ComPtr<ID3D12Resource>, kSwapChainResourcesCount>
      swapChainResources_ = {}; // レンダーターゲットビューの配列
  std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kSwapChainResourcesCount>
      rtvHandles_ = {}; // RTVのハンドル配列

  // フェンス
  ComPtr<ID3D12Fence> fence_ = nullptr; // GPU-CPU同期用
  uint64_t fenceValue_ = 0;     // GPUが次に書き込むフェンス値(CPUで保持)
  HANDLE fenceEvent_ = nullptr; // フェンスの待機イベント

  // ビューポート矩形
  D3D12_VIEWPORT viewport_ = {};

  // シザー矩形
  D3D12_RECT scissorRect_ = {};

  // DXCコンパイラ
  ComPtr<IDxcUtils> dxcUtils_ = nullptr;                // ユーティリティ
  ComPtr<IDxcCompiler3> dxcCompiler_ = nullptr;         // コンパイラ
  ComPtr<IDxcIncludeHandler> includeHandler_ = nullptr; // インクルードハンドラ
  bool isCommandListOpen_ = false;                      // コマンドリストの開閉状態

#pragma endregion privateメンバ変数

public:  // シングルトンインスタンス取得
    static DX12Context* GetInstance();
    // インスタンス破棄用関数
    static void Destroy();

public: // メンバ関数
#pragma region publicメンバ関数

  // コンストラクタ(隠蔽)
  explicit DX12Context(Token);
  // 初期化
  void Initialize();
  // 終了
  void Finalize();
  // 描画前処理
  void PreDraw();
  // 描画後処理
  void PostDraw();

  // コマンドリストをリセットして記録可能状態にする
  void ResetCommandList();
  // コマンドリストが開いている（記録可能状態）か
  bool IsCommandListOpen() const { return isCommandListOpen_; }

#pragma region ゲッター

  // 各種DirectXオブジェクトのゲッター

  /// <summary>
  /// RTVの指定したインデックスのCPUディスクリプタハンドルを取得
  /// </summary>
  /// <param name="index">取得するRTVのインデックス</param>
  D3D12_CPU_DESCRIPTOR_HANDLE GetRTVCPUDescriptorHandle(uint32_t index);

  /// <summary>
  /// DSVの指定したインデックスのCPUディスクリプタハンドルを取得
  /// </summary>
  /// <param name="index">取得するDSVのインデックス</param>
  D3D12_CPU_DESCRIPTOR_HANDLE GetDSVCPUDescriptorHandle(uint32_t index);

  // デバイスのゲッター
  ID3D12Device *GetDevice() const { return device_.Get(); }

  // 深度バッファリソースのゲッター
  ID3D12Resource *GetDepthStencilResource() const {
    return depthStencilResource_.Get();
  }

  // コマンドリストのゲッター
  ID3D12GraphicsCommandList *GetCommandList() const {
    return commandList_.Get();
  }

  /// <summary>
  /// スワップチェーンリソースのゲッター
  /// </summary>
  uint32_t GetSwapChainResourceCount() const {
    return kSwapChainResourcesCount;
  }

  ID3D12CommandAllocator *GetCommandAllocator() {
      return commandAllocator_.Get();
  }

  void WaitForGpu();

#pragma endregion

#pragma endregion publicメンバ関数

private: // メンバ関数
#pragma region privateメンバ関数

  // デバイスの初期化
  void InitializeDevice();
  // コマンド関連の初期化
  void InitializeCommand();
  // スワップチェーンの生成
  void CreateSwapChain();
  // 深度バッファの生成
  void CreateDepthBuffer();
  // 各種デスクリプタヒープの生成
  void CreateDescriptorHeaps();
  // レンダーターゲットビューの初期化
  void InitializeRenderTargetView();
  // 深度ステンシルビューの初期化
  void InitializeDepthStencilView();
  // フェンスの生成
  void CreateFence();
  // ビューポート矩形の初期化
  void InitializeViewportRect();
  // シザリング矩形の初期化
  void InitializeScissorRect();
  // DXCコンパイラの生成
  void CreateDXCCompiler();
  // ImGuiの初期化
  // void InitializeImGui();

#pragma region 60FPS固定用

  // FPS固定初期化
  void InitializeFixFPS();
  // FPS固定更新(待機処理のVSync待ちの直後に行う)
  void UpdateFixFPS();
  // 記録時間
  std::chrono::steady_clock::time_point reference_;

#pragma endregion 60FPS固定用

#pragma endregion privateメンバ関数

public: // ヘルパー関数
#pragma region publicヘルパー関数

  // コマンドリストの実行と待機
  void ExecuteInitialCommandAndSync();

  // 現在のバックバッファをレンダーターゲットとしてセットし、ビューポート等もリセットする
  void SetBackBufferAsRenderTarget();

  /// <summary>
  /// シェーダーをコンパイルする関数
  /// </summary>
  /// <param name="filePath">コンパイルするシェーダーファイルへのパス</param>
  /// <param name="profile">コンパイルに使用するProfile</param>
  /// <returns>シェーダーのコンパイル</returns>
  ComPtr<IDxcBlob> CompileShader(const std::wstring &filePath,
                                 const wchar_t *profile);

  /// <summary>
  /// バッファリソースの生成
  /// </summary>
  /// <param name="sizeInBytes">バッファのサイズ</param>
  /// <returns>生成したバッファリソース</returns>
  ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

  /// <summary>
  /// UAV対応バッファリソースの生成 (DEFAULTヒープ)
  /// </summary>
  /// <param name="sizeInBytes">バッファのサイズ</param>
  /// <returns>生成したUAVバッファリソース</returns>
  ComPtr<ID3D12Resource> CreateUAVBufferResource(size_t sizeInBytes);

  /// <summary>
  /// テクスチャリソースの生成
  /// </summary>
  /// <param name="metadata">metadata</param>
  /// <returns>生成したテクスチャリソース</returns>
  ComPtr<ID3D12Resource>
  CreateTextureResource(const DirectX::TexMetadata &metadata);

  /// <summary>
  /// テクスチャデータの転送
  /// </summary>
  /// <param name="texture">テクスチャリソース</param>
  /// <param name="mipImages">テクスチャデータ</param>
  /// <returns>転送に使用したアップロード用バッファリソース</returns>
  [[nodiscard]] // 戻り値を破棄しない
  ComPtr<ID3D12Resource>
  UploadTextureData(ComPtr<ID3D12Resource> texture,
                    const DirectX::ScratchImage &mipImages);

  /// <summary>
  /// デスクリプタヒープを生成する
  /// </summary>
  /// <param name="heapType">デスクリプタヒープのタイプ</param>
  /// <param name="numDescriptors">デスクリプタの数</param>
  /// <param name="shaderVisible">シェーダから見えるかどうか</param>
  /// <returns>生成したデスクリプタヒープへのポインタ</returns>
  ComPtr<ID3D12DescriptorHeap>
  CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors,
                       bool shaderVisible);

#pragma endregion publicヘルパー関数
private: // ヘルパー関数
#pragma region privateヘルパー関数

#pragma region ハンドルのゲッター

  /// <summary>
  /// 指定したインデックスのCPUデスクリプタハンドルを取得
  /// </summary>
  /// <param name="descriptorHeap">デスクリプタヒープへのポインタ</param>
  /// <param name="descriptorSize">各デスクリプタのサイズ</param>
  /// <param name="index">取得するデスクリプタのインデックス</param>
  /// <returns>指定したインデックスに対応する
  /// D3D12_CPU_DESCRIPTOR_HANDLE</returns>
  static D3D12_CPU_DESCRIPTOR_HANDLE
  GetCPUDescriptorHandle(ComPtr<ID3D12DescriptorHeap> &descriptorHeap,
                         uint32_t descriptorSize, uint32_t index);

  /// <summary>
  /// 指定したインデックスのGPUデスクリプタハンドルを取得
  /// </summary>
  /// <param name="descriptorHeap">デスクリプタヒープへのポインタ</param>
  /// <param name="descriptorSize">各デスクリプタのサイズ</param>
  /// <param name="index">取得するデスクリプタのインデックス</param>
  /// <returns>指定したインデックスに対応する
  /// D3D12_GPU_DESCRIPTOR_HANDLE</returns>
  static D3D12_GPU_DESCRIPTOR_HANDLE
  GetGPUDescriptorHandle(ComPtr<ID3D12DescriptorHeap> &descriptorHeap,
                         uint32_t descriptorSize, uint32_t index);

#pragma endregion ハンドルのゲッター

#pragma endregion privateヘルパー関数

  private: // コンストラクタ・デストラクタ・コピー禁止
    ~DX12Context() = default;
    DX12Context(const DX12Context &) = delete;
    const DX12Context &operator=(const DX12Context &) = delete;

    friend std::default_delete<DX12Context>;
};
