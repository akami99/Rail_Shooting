#include <cassert>
#include <filesystem>
#include <format>
#include <thread>

#include "DX12Context.h"
#include "Base/Win32Window.h"
#include "../../Core/Utility/Logger/Logger.h"
#include "../../Core/Utility/String/StringUtility.h"

#include "externals/DirectXTex/d3dx12.h"

// .lidはヘッダに書いてはいけない
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace Microsoft::WRL;
using namespace Logger;
using namespace StringUtility;

std::unique_ptr<DX12Context> DX12Context::instance_ = nullptr;

// シングルトンインスタンスの実装
DX12Context *DX12Context::GetInstance() {
    if (instance_ == nullptr) {
        instance_ = std::make_unique<DX12Context>(Token{});
    }
    return instance_.get();
}

void DX12Context::Destroy() {
    instance_.reset();
}

DX12Context::DX12Context(Token) {
    // コンストラクタ
}

#pragma region publicメンバ関数

// 初期化
void DX12Context::Initialize() {
  // FPS固定初期化
  InitializeFixFPS();

  // デバイスの生成
  InitializeDevice();

  // コマンド関連の生成
  InitializeCommand();

  // スワップチェーンの生成
  CreateSwapChain();

  // 深度バッファの生成
  CreateDepthBuffer();

  // 各種デスクリプタヒープの生成
  CreateDescriptorHeaps();

  // レンダーターゲットビューの初期化
  InitializeRenderTargetView();

  // 深度ステンシルビューの初期化
  InitializeDepthStencilView();

  // フェンスの生成
  CreateFence();

  // ビューポート矩形の初期化
  InitializeViewportRect();

  // シザー矩形の初期化
  InitializeScissorRect();

  // DCXコンパイラの生成
  CreateDXCCompiler();

  // 生成直後に一度閉じる
  // これにより「最初は Closed 状態」というルールが確定する
  HRESULT hr;
  hr = commandList_->Close();
  assert(SUCCEEDED(hr));
  isCommandListOpen_ = false;

  Log("INFO: Complete Initialize DX12Context!!!\n"); // 初期化完了のログをだす
}

// 終了
void DX12Context::Finalize() {
    // まずGPUの処理完了を待つ
    WaitForGpu();
    // 1. コマンドリスト関連
    commandList_.Reset();
    commandAllocator_.Reset();
    commandQueue_.Reset();

    // 2. スワップチェーンとそのリソース（重要！）
    // これらがDeviceを参照しているので、必ず先に消す
    swapChain_.Reset();
    for (auto& resource : swapChainResources_) {
        resource.Reset();
    }

    // 3. 深度バッファ（重要！）
    depthStencilResource_.Reset();

    // 4. デスクリプタヒープ（重要！）
    rtvDescriptorHeap_.Reset(); 
    dsvDescriptorHeap_.Reset(); 

    // 5. その他ツール類
    dxcUtils_.Reset();
    dxcCompiler_.Reset();
    includeHandler_.Reset();
    dxgiFactory_.Reset();

    // 6. フェンスイベント閉じる
    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }
    fence_.Reset();

    // 7. 最後に親玉であるデバイスを放す
    device_.Reset();

    Log("INFO: Complete Finalize DX12Context!!!\n");
}

#pragma region 描画処理

// 描画開始前処理
void DX12Context::PreDraw() {

  HRESULT hr;

  // コマンドリストをリセット
  hr = commandAllocator_->Reset();
  assert(SUCCEEDED(hr));

  // コマンドリストを記録可能な状態にリセット
  ResetCommandList();

  // バックバッファのインデックスを取得
  UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

  // TransitionBarrierの設定
  D3D12_RESOURCE_BARRIER barrier{};
  // バリアはTransition
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  // Noneにしておく
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  // バリアを張る対象のリソース。現在のバックバッファに対して行う
  barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
  // 遷移前（現在）のResourceState
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  // 遷移後のResourceState
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  // TransitionBarrierを張る
  commandList_->ResourceBarrier(1, &barrier);

  // 描画先のRTSとDSVを設定する
  commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex], false,
                                   &dsvHandle_);

  // 指定した色で画面全体をクリアする
  float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f}; // 青っぽい色。RGBAの順
  commandList_->ClearRenderTargetView(rtvHandles_[backBufferIndex], clearColor,
                                      0, nullptr);

  // 指定した深度で画面全体をクリアする
  commandList_->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f,
                                      0, 0, nullptr);

  // ビューポート領域を設定
  commandList_->RSSetViewports(1, &viewport_);

  // シザー矩形を設定
  commandList_->RSSetScissorRects(1, &scissorRect_);
}

// 描画終了後処理
void DX12Context::PostDraw() {

  // バックバッファのインデックスを取得
  UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

  // TransitionBarrierの設定
  D3D12_RESOURCE_BARRIER barrier{};
  // バリアの共通設定(preDrawと同じ)
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();

  // 画面に描く処理がすべて終わり、画面に移すので、状態を遷移
  // RenderTargetからPresentにする
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  // TransitionBarrierを張る
  commandList_->ResourceBarrier(1, &barrier);

  // コマンドリストの実行と完了まで待つ
  //ExecuteInitialCommandAndSync();

  // コマンドリストを閉じる (Open -> Closed)
  HRESULT hr = commandList_->Close();
  assert(SUCCEEDED(hr));

  // コマンドを実行させる (Closed -> Executing)
  ID3D12CommandList *commandLists[] = { commandList_.Get() };
  commandQueue_->ExecuteCommandLists(1, commandLists);

  // コマンドキューにシグナルを送信し、Fence値を更新
  fenceValue_++;
  hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
  assert(SUCCEEDED(hr));

  // Fenceの値が指定したSignal値にたどり着いているか確認する
  if (fence_->GetCompletedValue() < fenceValue_) {
      // GPUがSignalを完了するまで待機するようにイベントを設定し、待つ
      hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
      assert(SUCCEEDED(hr));
      // イベントを待つ (この関数で Present の完了を待機し、FPS固定と同期を行う)
      WaitForSingleObject(fenceEvent_, INFINITE);
  }

  // FPS固定用の更新（これはどこでも良い）
  UpdateFixFPS();

  // Present
  swapChain_->Present(1, 0);
}

#pragma endregion 描画処理

#pragma region ゲッター

D3D12_CPU_DESCRIPTOR_HANDLE
DX12Context::GetRTVCPUDescriptorHandle(uint32_t index) {
  return GetCPUDescriptorHandle(rtvDescriptorHeap_, descriptorSizeRTV_, index);
}

D3D12_CPU_DESCRIPTOR_HANDLE
DX12Context::GetDSVCPUDescriptorHandle(uint32_t index) {
  return GetCPUDescriptorHandle(dsvDescriptorHeap_, descriptorSizeDSV_, index);
}

#pragma endregion ゲッター

void DX12Context::WaitForGpu() {
    // コマンドリストを閉じて実行（もし開いていれば）
    // ※ 実行中のコマンドがないなら、単にFenceで待つだけでもOK

    // フェンス値を更新してシグナルを送る
    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);

    // フェンス値が到達するまで待つ
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

#pragma endregion publicメンバ関数

#pragma region privateメンバ関数

// デバイスの初期化
void DX12Context::InitializeDevice() {

  HRESULT hr;

#ifdef _DEBUG
  ComPtr<ID3D12Debug1> debugController = nullptr;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    // デバッグレイヤーを有効化する
    debugController->EnableDebugLayer();
    // さらにGPU側でもチェックを行うようにする
    debugController->SetEnableGPUBasedValidation(TRUE);
  }
#endif

  // DXGIファクトリーの生成

  // HRESULTはWindows系のエラーコードであり、
  // 関数が成功したかどうかをSUCCEEDEDマクロで判定できる
  hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
  // 　初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、
  //  どうにもできない場合が多いのでassertにしておく
  assert(SUCCEEDED(hr));

  // 使用するアダプタ用の変数。最初にnullptrを入れておく
  ComPtr<IDXGIAdapter4> useAdapter = nullptr;
  // 良い順にアダプタを頼む
  for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(
                       i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                       IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND;
       i++) {
    // アダプタの情報を取得する
    DXGI_ADAPTER_DESC3 adapterDesc{};
    hr = useAdapter->GetDesc3(&adapterDesc);
    assert(SUCCEEDED(hr)); // 取得できないのは一大事
    // ソフトウェアアダプタでなければ採用!
    if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
      // 採用したアダプタの情報をログに出力。wstringの方なので注意
      Log(ConvertString(
          std::format(L"Use Adapter: {}\n", adapterDesc.Description)));
      break;
    }
    useAdapter = nullptr; // ソフトウェアアダプタ場合は見なかったことにする
  }
  // 適切なアダプタが見つからなかったので起動できない
  assert(useAdapter != nullptr);

  // 機能レベルとログ出力用の文字列
  D3D_FEATURE_LEVEL featureLevels[] = {
      D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};
  const char *featureLevelStrings[] = {"12.2", "12.1", "12.0"};
  // 高い順に生成できるか試していく
  for (size_t i = 0; i < _countof(featureLevels); ++i) {
    // 採用したアダプターでデバイスを生成
    hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i],
                           IID_PPV_ARGS(&device_));
    // 指定した機能レベルでデバイスが生成できたかを確認
    if (FAILED(hr)) {
      // 重要なデバッグログをファイルに書き出す
      Logger::Log(std::format(
          "FATAL ERROR: D3D12CreateDevice failed. HRESULT: 0x{:08X}",
          (uint32_t)hr));
      // D3D12CreateDeviceが失敗した場合、デバイス対応がないか、ドライバが古い
      assert(false && "Failed to create D3D12 Device.");
      // ここで強制的に終了する
      exit(1);
    }
    if (SUCCEEDED(hr)) {
      // 生成できたのでログ出力を行ってループを抜ける
      Log(std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
      break;
    }
  }
  // デバイスが生成が上手くいかなかったので起動できない
  assert(device_ != nullptr);
  Log("INFO: Complete create D3D12Device!!!\n"); // 初期化完了のログをだす

#ifdef _DEBUG
  ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
  if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
    // やばいエラー時に止まる
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    // エラー時に止まる
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
    // 警告時に止まる。※解放を忘れた際は絶対に戻すように気を付けつつコメントアウトしてログを見る
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
    // 抑制するメッセージのID
    D3D12_MESSAGE_ID denyIds[] = {
        // Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
        // https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
        D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE};
    // 抑制するレベル
    D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};
    D3D12_INFO_QUEUE_FILTER filter{};
    filter.DenyList.NumIDs = _countof(denyIds);
    filter.DenyList.pIDList = denyIds;
    filter.DenyList.NumSeverities = _countof(severities);
    filter.DenyList.pSeverityList = severities;
    // 指定したメッセージの表示を抑制する
    infoQueue->PushStorageFilter(&filter);
  }
#endif
}

// コマンド関連の初期化
void DX12Context::InitializeCommand() {
  HRESULT hr;

  // コマンドアロケータを生成する
  hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       IID_PPV_ARGS(&commandAllocator_));
  // コマンドアロケータの生成が上手くいかなかったので起動できない
  assert(SUCCEEDED(hr));

  // コマンドキューを生成する
  D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
  hr = device_->CreateCommandQueue(&commandQueueDesc,
                                   IID_PPV_ARGS(&commandQueue_));
  // コマンドキューの生成が上手くいかなかったので起動できない
  assert(SUCCEEDED(hr));

  // コマンドリストを生成する
  hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  commandAllocator_.Get(), nullptr,
                                  IID_PPV_ARGS(&commandList_));
  // コマンドリストの生成が上手くいかなかったので起動できない
  assert(SUCCEEDED(hr));
  isCommandListOpen_ = true;
}

// スワップチェーンの初期化
void DX12Context::CreateSwapChain() {
  HRESULT hr;

  // スワップチェーン生成の設定
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
  swapChainDesc.Width = Win32Window::
      kClientWidth; // 画面の幅。ウィンドウのクライアント領域を同じものにしておく
  swapChainDesc.Height = Win32Window::
      kClientHeight; // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
  swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
  swapChainDesc.SampleDesc.Count = 1;                // マルチサンプルしない
  swapChainDesc.BufferUsage =
      DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
  swapChainDesc.BufferCount = 2;       // ダブルバッファ
  swapChainDesc.SwapEffect =
      DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニタに移したら、中身を破棄

  // コマンドキュー、ウィンドウハンドル、設定を渡して生成する
  hr = dxgiFactory_->CreateSwapChainForHwnd(
      commandQueue_.Get(), Win32Window::GetInstance()->GetHwnd(), &swapChainDesc, nullptr, nullptr,
      reinterpret_cast<IDXGISwapChain1 **>(swapChain_.GetAddressOf()));
  assert(SUCCEEDED(hr));
}

// 深度バッファの生成
void DX12Context::CreateDepthBuffer() {
  // ウィンドウサイズを取得
  uint32_t width = Win32Window::kClientWidth;
  uint32_t height = Win32Window::kClientHeight;

  HRESULT hr;

  // Resourceの設定
  D3D12_RESOURCE_DESC resourceDesc{};
  resourceDesc.Width = width;        // Textureの幅
  resourceDesc.Height = height;      // Textureの高さ
  resourceDesc.MipLevels = 1;        // mipmapの数
  resourceDesc.DepthOrArraySize = 1; // 奥行き or 配列Textureの配列数
  resourceDesc.Format =
      DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
  resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定。
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
  resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // 標準的なレイアウト
  resourceDesc.Flags =
      D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

  // 深度値のクリア設定
  D3D12_CLEAR_VALUE depthClearValue{};
  depthClearValue.DepthStencil.Depth = 1.0f; // 1.0f（最大値）でクリア
  depthClearValue.Format =
      DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット。Resourceと合わせる

  // 利用するHeapの設定
  D3D12_HEAP_PROPERTIES heapProps{};
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

  // Resourceの生成
  hr = device_->CreateCommittedResource(
      &heapProps,                       // Heapの設定
      D3D12_HEAP_FLAG_NONE,             // Heapの特殊な設定。特になし。
      &resourceDesc,                    // Resourceの設定
      D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
      &depthClearValue,                 // Clear最適値。
      IID_PPV_ARGS(
          &depthStencilResource_)); // 作成するResourceポインタへのポインタ
  assert(SUCCEEDED(hr));
}

// 各種デスクリプタヒープの生成
void DX12Context::CreateDescriptorHeaps() {
  // DiscriptorSizeを取得しておく
  descriptorSizeRTV_ =
      device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  descriptorSizeDSV_ =
      device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

  // RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisubleはfalse
  rtvDescriptorHeap_ =
      CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
  // DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、ShaderVisubleはfalse
  dsvDescriptorHeap_ =
      CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
}

// レンダーターゲットビューの初期化
void DX12Context::InitializeRenderTargetView() {
  HRESULT hr;

  // SwapChainからResourceを引っ張ってくる
  for (uint32_t i = 0; i < kSwapChainResourcesCount; ++i) {
    hr = swapChain_->GetBuffer(
        i, IID_PPV_ARGS(swapChainResources_[i].ReleaseAndGetAddressOf()));
    // うまく取得できなければ起動できない
    assert(SUCCEEDED(hr));
  }

  // RTVの設定
  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
  rtvDesc.Format =
      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換して書き込む
  rtvDesc.ViewDimension =
      D3D12_RTV_DIMENSION_TEXTURE2D; // 2dテクスチャとして書き込む

  // ディスクリプタの先頭を取得する
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
      rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

  // ディスクリプタのサイズを取得する
  uint32_t rtvDescriptorSize =
      device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  // 裏表の2つ分
  for (uint32_t i = 0; i < kSwapChainResourcesCount; ++i) {
    // i番目のレンダーターゲットリソースを取得
    device_->CreateRenderTargetView(swapChainResources_[i].Get(), &rtvDesc,
                                    rtvHandle);

    // 作成したRTVに対応するCPUハンドルをメンバ変数に保存
    rtvHandles_[i] = rtvHandle;

    // 次のディスクリプタの場所に移動
    rtvHandle.ptr += rtvDescriptorSize;
  }
}

// 深度ステンシルビューの初期化
void DX12Context::InitializeDepthStencilView() {

  // DSVの設定
  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
  dsvDesc.Format =
      DXGI_FORMAT_D24_UNORM_S8_UINT; // Format。基本的にはResourceに合わせる
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2dTexture
  dsvDesc.Flags = D3D12_DSV_FLAG_NONE;                   // 特殊な設定はなし

  // DSVハンドルを取得
  dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

  // DSVの生成
  device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc,
                                  dsvHandle_);
}

// フェンスの生成
void DX12Context::CreateFence() {
  HRESULT hr;

  // 初期値０でFenceを作る
  hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE,
                            IID_PPV_ARGS(&fence_));
  assert(SUCCEEDED(hr));

  // FenceのSignalを持つためのイベントを作成する
  fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
  assert(fenceEvent_ != nullptr);
}

// ビューポート矩形の初期化
void DX12Context::InitializeViewportRect() {

  // クライアント領域のサイズと一緒にして画面全体に表示
  viewport_.Width = Win32Window::kClientWidth;
  viewport_.Height = Win32Window::kClientHeight;
  viewport_.TopLeftX = 0;
  viewport_.TopLeftY = 0;
  viewport_.MinDepth = 0.0f;
  viewport_.MaxDepth = 1.0f;
}

// シザー矩形の初期化
void DX12Context::InitializeScissorRect() {

  // 基本的にビューポートと同じ矩形が構成されるようにする
  scissorRect_.left = 0;
  scissorRect_.top = 0;
  scissorRect_.right = Win32Window::kClientWidth;
  scissorRect_.bottom = Win32Window::kClientHeight;
}

// DCXコンパイラの生成
void DX12Context::CreateDXCCompiler() {
  HRESULT hr;

  // dxcCompilerを初期化
  hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
  assert(SUCCEEDED(hr));
  hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
  assert(SUCCEEDED(hr));

  // includeに対応するための設定
  hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
  assert(SUCCEEDED(hr));
}

#pragma region 60FPS固定用

void DX12Context::InitializeFixFPS() {
  // 現在時刻を記録する
  reference_ = std::chrono::steady_clock::now();
}

void DX12Context::UpdateFixFPS() {
  // 1/60秒の時間
  const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));

  // 1/60秒よりわずかに短い時間
  const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f / 65.0f));

  // 現在時刻を取得する
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  // 前回記録からの経過時間を取得する
  std::chrono::microseconds elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

  // 1/60秒(より短い時間)たっていない場合
  if (elapsed < kMinCheckTime) {
    // 1/60秒経過するまで微小なスリープを繰り返す
    while (std::chrono::steady_clock::now() - reference_ < kMinTime) {
      // 1マイクロ秒スリープ
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
  }
  // 現在の時間を記録する
  reference_ = std::chrono::steady_clock::now();
}

#pragma endregion 60FPS固定用

#pragma endregion privateメンバ関数

#pragma region publicヘルパー関数

// コマンドリストの実行と完了まで待つ
void DX12Context::ExecuteInitialCommandAndSync() {

  HRESULT hr;

  // コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
  hr = commandList_->Close();
  assert(SUCCEEDED(hr));
  isCommandListOpen_ = false;

  // GPUにコマンドリストの実行を行わせる
  ID3D12CommandList *commandLists[] = {commandList_.Get()};
  commandQueue_->ExecuteCommandLists(1, commandLists);

  WaitForGpu();

  // 次のフレーム用のコマンドリストを準備
  hr = commandAllocator_->Reset();
  assert(SUCCEEDED(hr));

  // 中間リソースの解放
  // intermediateResources_.clear();
}

void DX12Context::ResetCommandList() {
  HRESULT hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
  assert(SUCCEEDED(hr));
  isCommandListOpen_ = true;
}

void DX12Context::SetBackBufferAsRenderTarget() {
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    // レンダーターゲットをバックバッファに設定
    commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex], false, &dsvHandle_);
    // ビューポートとシザー矩形をデフォルト（画面全体）に戻す
    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);
}

// シェーダーのコンパイル
ComPtr<IDxcBlob> DX12Context::CompileShader(const std::wstring &filePath,
                                            const wchar_t *profile) {

  HRESULT hr;

  std::wstring fullPath = filePath;
  // const std::wstring prefix = L"DirectXGame/Resources/Shaders/";
  //  既にパスに"DirectXGame/Resources/Shaders/"が含まれている場合は追加しない
  if (filePath.find(L"Resources/Shaders/") == std::wstring::npos &&
      filePath.find(L"resources/shaders/") == std::wstring::npos) {
    fullPath = L"Resources/Shaders/" + filePath;
  }

  // これからシェーダーをコンパイルする旨をログに出す
  Log(ConvertString(std::format(L"INFO: Begin CompileShader, path:{}, profile:{}\n",
                                fullPath, profile)));
  // hlslファイルを読む
  ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
  hr = dxcUtils_->LoadFile(fullPath.c_str(), nullptr, &shaderSource);
  // 読めなかったら止める
  if (FAILED(hr)) {
    // 1. エラーBLOBの取得（D3DCompileの場合）
    ID3DBlob *errorBlob = nullptr; // D3DCompileの引数から取得

    // 2. エラーメッセージのログ出力
    if (errorBlob) {
      Logger::Log("ERROR: " + std::string((char *)errorBlob->GetBufferPointer()));
      errorBlob->Release();
    } else {
      // hrの値をログに出力する（DXCの場合など）
      Logger::Log("ERROR: Shader compilation failed with HRESULT: " +
                  std::to_string(hr));
    }
  }
  assert(SUCCEEDED(hr));
  // 読み込んだファイルの内容を設定する
  DxcBuffer shaderSourceBuffer;
  shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
  shaderSourceBuffer.Size = shaderSource->GetBufferSize();
  shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8の文字コードであることを通知

  // Compileする
  LPCWSTR arguments[] = {
      fullPath.c_str(), // コンパイル対象のhlslファイル名
      L"-E",
      L"main", // エントリーポイントの指定。基本的にmain以外にはしない
      L"-T",
      profile, // ShaderProfileの設定
      L"-Zi",
      L"-Qembed_debug", // デバッグ用の情報を埋め込む
      L"-Od",           // 最適化を外しておく
      L"-Zpr",          // メモリレイアウトは行優先
  };
  // 実際にShaderをコンパイルする
  ComPtr<IDxcResult> shaderResult = nullptr;
  hr = dxcCompiler_->Compile(&shaderSourceBuffer,   // 読み込んだファイル
                             arguments,             // コンパイルオプション
                             _countof(arguments),   // コンパイルオプションの数
                             includeHandler_.Get(), // includeが含まれた諸々
                             IID_PPV_ARGS(&shaderResult) // コンパイル結果
  );
  // コンパイルエラーではなくdxcが起動できないなど致命的な状況
  assert(SUCCEEDED(hr));

  // 警告・エラーがでていないか確認する
  // 警告・エラーが出てたらログに出して止める
  ComPtr<IDxcBlobUtf8> shaderError = nullptr;
  shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
  if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
    Log("ERROR: " + std::string(shaderError->GetStringPointer()));
    // 警告・エラーダメゼッタイ
    assert(false);
  }

  // Compile結果を受け取って返す
  // コンパイル結果から実行用のバイナリ部分を取得
  ComPtr<IDxcBlob> shaderBlob = nullptr;
  hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob),
                               nullptr);
  assert(SUCCEEDED(hr));
  // 成功したログを出す(性能を重視する場合は少しオーバーヘッドがあるので注意)
  Log(ConvertString(std::format(L"INFO: CompileSucceded, path:{}, profile:{}\n",
                                fullPath, profile)));
  // 実行用のバイナリを返却
  return shaderBlob;
}

// バッファリソースの生成
ComPtr<ID3D12Resource> DX12Context::CreateBufferResource(size_t sizeInBytes) {

    // ★追加: サイズが0ならエラーになるので、最低でも1バイト（現実的にはアラインメントで増える）確保する
    if (sizeInBytes == 0) {
        // 本来は呼び出し元で対処すべきだが、クラッシュ回避のために1にするか、あるいはここでassertしてもよい
        // sizeInBytes = 1; 
        assert(false && "CreateBufferResource called with size 0");
    }

    // ★追加: サイズを256バイトの倍数に切り上げる
    // (ConstantBufferViewとしても使う可能性がある場合、これは必須です。IndexBufferとしても安全です)
    size_t alignedSize = (sizeInBytes + 0xff) & ~0xff;

  HRESULT hr;

  // ヒープ設定（UploadHeap）
  D3D12_HEAP_PROPERTIES uploadHeapProperties{};
  uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

  // リソース設定（バッファ用）
  D3D12_RESOURCE_DESC vertexResourceDesc{};
  vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  vertexResourceDesc.Width = sizeInBytes;
  vertexResourceDesc.Height = 1;
  vertexResourceDesc.DepthOrArraySize = 1;
  vertexResourceDesc.MipLevels = 1;
  vertexResourceDesc.SampleDesc.Count = 1;
  vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  // リソース作成
  ComPtr<ID3D12Resource> vertexResource = nullptr;
  hr = device_->CreateCommittedResource(
      &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&vertexResource));
  assert(SUCCEEDED(hr));

  // リークが分からない場合にのみ使う追跡用コード: 生成されたリソースのアドレスをログに出す
  /*Logger::Log(std::format("Resource CREATED: Ptr = 0x{:X}, Size = {}\n",
      reinterpret_cast<uint64_t>(vertexResource.Get()), sizeInBytes));*/

  return vertexResource;
}

// UAV対応バッファリソースの生成
ComPtr<ID3D12Resource> DX12Context::CreateUAVBufferResource(size_t sizeInBytes) {
  if (sizeInBytes == 0) {
    assert(false && "CreateUAVBufferResource called with size 0");
  }

  size_t alignedSize = (sizeInBytes + 0xff) & ~0xff;
  HRESULT hr;

  // ヒープ設定 (DefaultHeap: GPU専用)
  D3D12_HEAP_PROPERTIES defaultHeapProperties{};
  defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

  // リソース設定 (UAVアクセス許可)
  D3D12_RESOURCE_DESC bufferDesc{};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Width = alignedSize;
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

  ComPtr<ID3D12Resource> bufferResource = nullptr;
  hr = device_->CreateCommittedResource(
      &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
      D3D12_RESOURCE_STATE_COMMON, nullptr,
      IID_PPV_ARGS(&bufferResource));
  assert(SUCCEEDED(hr));

  return bufferResource;
}

#pragma endregion publicヘルパー関数

#pragma region privateヘルパー関数

// ディスクリプタヒープの生成
ComPtr<ID3D12DescriptorHeap>
DX12Context::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType,
                                  UINT numDescriptors, bool shaderVisible) {
  HRESULT hr;

  // ディスクリプタヒープの生成
  ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
  D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
  descriptorHeapDesc.Type = heapType; // レンダーターゲットビュー用
  descriptorHeapDesc.NumDescriptors = numDescriptors; // 多くても別に構わない
  descriptorHeapDesc.Flags = shaderVisible
                                 ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                 : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  hr = device_->CreateDescriptorHeap(&descriptorHeapDesc,
                                     IID_PPV_ARGS(&descriptorHeap));
  // ディスクリプタヒープの生成が作れなかったので起動できない
  assert(SUCCEEDED(hr));
  return descriptorHeap;
}

#pragma region ハンドルのゲッター

// CPUHandleの取得
D3D12_CPU_DESCRIPTOR_HANDLE DX12Context::GetCPUDescriptorHandle(
    ComPtr<ID3D12DescriptorHeap> &descriptorHeap, uint32_t descriptorSize,
    uint32_t index) {
  D3D12_CPU_DESCRIPTOR_HANDLE handleCPU =
      descriptorHeap->GetCPUDescriptorHandleForHeapStart();
  handleCPU.ptr += (descriptorSize * index);
  return handleCPU;
}

// GPUHandleの取得
D3D12_GPU_DESCRIPTOR_HANDLE DX12Context::GetGPUDescriptorHandle(
    ComPtr<ID3D12DescriptorHeap> &descriptorHeap, uint32_t descriptorSize,
    uint32_t index) {
  D3D12_GPU_DESCRIPTOR_HANDLE handleGPU =
      descriptorHeap->GetGPUDescriptorHandleForHeapStart();
  handleGPU.ptr += (descriptorSize * index);
  return handleGPU;
}

#pragma endregion ハンドルのゲッター

// テクスチャデータの転送
ComPtr<ID3D12Resource>
DX12Context::UploadTextureData(ComPtr<ID3D12Resource> texture,
                               const DirectX::ScratchImage &mipImages) {

  std::vector<D3D12_SUBRESOURCE_DATA> subresources;
  DirectX::PrepareUpload(device_.Get(), mipImages.GetImages(),
                         mipImages.GetImageCount(), mipImages.GetMetadata(),
                         subresources);
  uint64_t intermediateSize =
      GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
  ComPtr<ID3D12Resource> intermediateResource =
      CreateBufferResource(intermediateSize);

  UpdateSubresources(commandList_.Get(), texture.Get(),
                     intermediateResource.Get(), 0, 0,
                     UINT(subresources.size()), subresources.data());

  // Textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = texture.Get();
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
  commandList_->ResourceBarrier(1, &barrier);
  return intermediateResource;
}

#pragma endregion privateヘルパー関数

// テクスチャリソースの生成
ComPtr<ID3D12Resource>
DX12Context::CreateTextureResource(const DirectX::TexMetadata &metadata) {

  HRESULT hr;

  // metadataを基にResourceの設定
  D3D12_RESOURCE_DESC resourceDesc{};
  resourceDesc.Width = UINT(metadata.width);           // Textureの幅
  resourceDesc.Height = UINT(metadata.height);         // Textureの高さ
  resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipmapの数
  resourceDesc.DepthOrArraySize =
      UINT16(metadata.arraySize);        // 奥行き or 配列Textureの配列数
  resourceDesc.Format = metadata.format; // TextureのFormat
  resourceDesc.SampleDesc.Count = 1;     // サンプリングカウント。1固定。
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(
      metadata.dimension); // Textureの次元数。普段使っているのは2次元

  // 利用するHeapの設定
  D3D12_HEAP_PROPERTIES heapProperties{};
  heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

  // Resourceを生成する
  ComPtr<ID3D12Resource> resource = nullptr;
  hr = device_->CreateCommittedResource(
      &heapProperties,                // Heapの設定
      D3D12_HEAP_FLAG_NONE,           // Heapの特殊な設定。特になし。
      &resourceDesc,                  // Resourceの設定
      D3D12_RESOURCE_STATE_COPY_DEST, // データ転送される設定(03_00ex)
      nullptr,                        // Clear最適値。使わないでnullptr
      IID_PPV_ARGS(&resource));       // 作成するResourceポインタへのポインタ
  assert(SUCCEEDED(hr));
  return resource;
}
