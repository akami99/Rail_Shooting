#include "MyGame.h"
#include <chrono>

// フレームワーク基盤
#include "RAFramework.h"
// エンジン
#include "DX12Context.h"
// 管理系
#include "ImGuiManager.h"
#include "LightManager.h"
#include "ParticleManager.h"
#include "PipelineManager.h"
#include "PostProcessManager.h"
#include "SceneManager.h"
#include "SrvManager.h"
#include "TextureManager.h"
// Scene
#include "SceneFactory.h"

#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

void MyGame::Initialize() {
  RAFramework::Initialize();

  	// 閉じているコマンドリストを再度開く（テクスチャロード等のために必要）
	DX12Context::GetInstance()->ResetCommandList();

  // オフスクリーンレンダリング用のリソース初期化
  renderTexture_ = std::make_unique<RenderTexture>();
  renderTexture_->Initialize(1280, 720);

  // 後の初期化でメタデータを参照するため、先にロードしておく
  TextureManager::GetInstance()->LoadTexture("uvChecker.png");
  TextureManager::GetInstance()->LoadTexture("masks/noise0.png");
  TextureManager::GetInstance()->LoadTexture("masks/noise1.png");
  // デフォルトテクスチャのロード（未ロード時の代替用）
  TextureManager::GetInstance()->LoadTexture(
      TextureManager::GetInstance()->defaultTexturePath_);

  // ポストプロセスマネージャの初期化
  PostProcessManager::GetInstance()->Initialize();

  // 追加した初期化コマンド（テクスチャ転送等）を実行し、GPUの完了を待つ
  DX12Context::GetInstance()->ExecuteInitialCommandAndSync();

  // ファクトリーを作成し、SceneManagerにセット
  auto sceneFactory = std::make_unique<SceneFactory>();
  SceneManager::GetInstance()->SetSceneFactory(std::move(sceneFactory));

  // 文字列で指定(TITLE/GAMEPLAY/SHOOTINGなど)してシーン切り替え予約
#ifdef USE_IMGUI
  SceneManager::GetInstance()->ChangeScene("SHOOTING");
#else
  SceneManager::GetInstance()->ChangeScene("TITLE");
#endif
}

void MyGame::Finalize() {
  PostProcessManager::Destroy();
  RAFramework::Finalize();
}

void MyGame::Update() {
  // DeltaTimeの計算 (フレームレート非依存)
  static auto lastTime = std::chrono::steady_clock::now();
  auto currentTime = std::chrono::steady_clock::now();
  std::chrono::duration<float> elapsed = currentTime - lastTime;
  lastTime = currentTime;
  float deltaTime = elapsed.count();

  // ポストプロセス更新 (ロジック更新)
  PostProcessManager::GetInstance()->Update(deltaTime);

  // ImGuiの受付開始
  imGuiManager_->Begin();

  // シーンマネージャに現在のシーンを更新させる
  SceneManager::GetInstance()->Update();

  // ライト更新
  LightManager::GetInstance()->Update();

#ifdef USE_IMGUI
  // ポストエフェクトモード選択 ImGui
  ImGui::SetNextWindowPos(ImVec2(10.0f, 210.0f), ImGuiCond_Once);
  ImGui::SetNextWindowSize(ImVec2(280.0f, 150.0f), ImGuiCond_Once);
  ImGui::Begin("PostEffect");
  int mode = PostProcessManager::GetInstance()->GetCurrentMode();
  if (ImGui::Combo(
          "Mode", &mode,
          "Copy (None)\0GrayScale\0Sepia\0Vignette\0Smoothing\0Gaussian "
          "Blur\0Outline\0Radial Blur\0Dissolve\0Random\0HSV\0")) {
    PostProcessManager::SetMode(mode);
  }
  if (mode == PostProcessManager::kModeVignette) {
    static float scale = 16.0f;
    static float exponent = 0.8f;
    bool changed = false;
    if (ImGui::SliderFloat("Vignette Scale", &scale, PostProcessManager::kMinVignetteScale, PostProcessManager::kMaxVignetteScale)) {
      changed = true;
    }
    if (ImGui::SliderFloat("Vignette Exponent", &exponent, PostProcessManager::kMinVignetteExponent, PostProcessManager::kMaxVignetteExponent)) {
      changed = true;
    }
    if (changed) {
      PostProcessManager::SetVignetteParams(scale, exponent);
      scale = PostProcessManager::GetVignetteScale();
      exponent = PostProcessManager::GetVignetteExponent();
    }
  } else if (mode == PostProcessManager::kModeSmoothing) {
    static int kernelSize = 3;
    if (ImGui::SliderInt("Kernel Size", &kernelSize, PostProcessManager::kMinSmoothingKernelSize, PostProcessManager::kMaxSmoothingKernelSize)) {
      PostProcessManager::SetSmoothingParams(kernelSize);
      kernelSize = PostProcessManager::GetSmoothingKernelSize();
    }
  } else if (mode == PostProcessManager::kModeGaussianBlur) {
    static int kernelSize = 3;
    static float sigma = 2.0f;
    bool changed = false;
    if (ImGui::SliderInt("Kernel Size", &kernelSize, PostProcessManager::kMinGaussianBlurKernelSize, PostProcessManager::kMaxGaussianBlurKernelSize)) {
      changed = true;
    }
    if (ImGui::SliderFloat("Sigma", &sigma, PostProcessManager::kMinGaussianBlurSigma, PostProcessManager::kMaxGaussianBlurSigma)) {
      changed = true;
    }
    if (changed) {
      PostProcessManager::SetGaussianBlurParams(kernelSize, sigma);
      kernelSize = PostProcessManager::GetGaussianBlurKernelSize();
      sigma = PostProcessManager::GetGaussianBlurSigma();
    }
  } else if (mode == PostProcessManager::kModeOutline) {
    static float depthEdgeMultiplier = 6.0f;
    static float colorEdgeMultiplier = 1.0f;
    bool changed = false;
    if (ImGui::SliderFloat("Depth Edge Multiplier", &depthEdgeMultiplier, PostProcessManager::kMinOutlineDepthEdgeMultiplier, PostProcessManager::kMaxOutlineDepthEdgeMultiplier)) {
      changed = true;
    }
    if (ImGui::SliderFloat("Color Edge Multiplier", &colorEdgeMultiplier, PostProcessManager::kMinOutlineColorEdgeMultiplier, PostProcessManager::kMaxOutlineColorEdgeMultiplier)) {
      changed = true;
    }
    if (changed) {
      PostProcessManager::SetOutlineParams(depthEdgeMultiplier, colorEdgeMultiplier);
      depthEdgeMultiplier = PostProcessManager::GetOutlineDepthEdgeMultiplier();
      colorEdgeMultiplier = PostProcessManager::GetOutlineColorEdgeMultiplier();
    }
  } else if (mode == PostProcessManager::kModeRadialBlur) {
    static float center[2] = { 0.5f, 0.5f };
    static float blurWidth = 0.01f;
    static int sampleCount = 10;
    bool changed = false;
    if (ImGui::SliderFloat2("Center", center, PostProcessManager::kMinRadialBlurCenterX, PostProcessManager::kMaxRadialBlurCenterX)) {
      changed = true;
    }
    if (ImGui::SliderFloat("Blur Width", &blurWidth, PostProcessManager::kMinRadialBlurWidth, PostProcessManager::kMaxRadialBlurWidth)) {
      changed = true;
    }
    if (ImGui::SliderInt("Sample Count", &sampleCount, PostProcessManager::kMinRadialBlurSampleCount, PostProcessManager::kMaxRadialBlurSampleCount)) {
      changed = true;
    }
    if (changed) {
      PostProcessManager::SetRadialBlurParams(center[0], center[1], blurWidth, sampleCount);
      center[0] = PostProcessManager::GetRadialBlurCenterX();
      center[1] = PostProcessManager::GetRadialBlurCenterY();
      blurWidth = PostProcessManager::GetRadialBlurWidth();
      sampleCount = PostProcessManager::GetRadialBlurSampleCount();
    }
  } else if (mode == PostProcessManager::kModeDissolve) {
    static float edgeColor[3] = { 1.0f, 0.4f, 0.3f };
    static float threshold = 0.0f;
    static float edgeRange = 0.02f;
    static int maskIndex = 0;
    const char* maskPaths[] = { "masks/noise0.png", "masks/noise1.png" };
    const char* maskNames[] = { "noise0", "noise1" };
    bool changed = false;
    if (ImGui::SliderFloat("Threshold", &threshold, PostProcessManager::kMinDissolveThreshold, PostProcessManager::kMaxDissolveThreshold)) {
      changed = true;
    }
    if (ImGui::ColorEdit3("Edge Color", edgeColor)) {
      changed = true;
    }
    if (ImGui::SliderFloat("Edge Range", &edgeRange, PostProcessManager::kMinDissolveEdgeRange, PostProcessManager::kMaxDissolveEdgeRange)) {
      changed = true;
    }
    if (ImGui::Combo("Mask Texture", &maskIndex, maskNames, 2)) {
      PostProcessManager::SetDissolveMaskTexture(maskPaths[maskIndex]);
    }
    if (changed) {
      PostProcessManager::SetDissolveParams(edgeColor, threshold, edgeRange);
      const float* col = PostProcessManager::GetDissolveEdgeColor();
      edgeColor[0] = col[0];
      edgeColor[1] = col[1];
      edgeColor[2] = col[2];
      threshold = PostProcessManager::GetDissolveThreshold();
      edgeRange = PostProcessManager::GetDissolveEdgeRange();
    }
  } else if (mode == PostProcessManager::kModeRandom) {
    static float randomStrength = 0.0f;
    if (ImGui::SliderFloat("Random Strength", &randomStrength, PostProcessManager::kMinRandomStrength, PostProcessManager::kMaxRandomStrength)) {
      PostProcessManager::SetRandomParams(randomStrength);
      randomStrength = PostProcessManager::GetRandomStrength();
    }
  } else if (mode == PostProcessManager::kModeHSV) {
    static float hue = 0.0f;
    static float saturation = 0.0f;
    static float value = 0.0f;
    bool changed = false;
    if (ImGui::SliderFloat("Hue", &hue, PostProcessManager::kMinHSVHue, PostProcessManager::kMaxHSVHue)) {
      changed = true;
    }
    if (ImGui::SliderFloat("Saturation", &saturation, PostProcessManager::kMinHSVSaturation, PostProcessManager::kMaxHSVSaturation)) {
      changed = true;
    }
    if (ImGui::SliderFloat("Value", &value, PostProcessManager::kMinHSVValue, PostProcessManager::kMaxHSVValue)) {
      changed = true;
    }
    if (changed) {
      PostProcessManager::SetHSVParams(hue, saturation, value);
      hue = PostProcessManager::GetHSVHue();
      saturation = PostProcessManager::GetHSVSaturation();
      value = PostProcessManager::GetHSVValue();
    }
  }
  ImGui::End();
#endif

  imGuiManager_->End();
}

void MyGame::Draw() {
  ID3D12GraphicsCommandList *commandList =
      DX12Context::GetInstance()->GetCommandList();

  // 描画前処理（コマンドリストのリセット、バックバッファをセット）
  DX12Context::GetInstance()->PreDraw();
  SrvManager::GetInstance()->PreDraw();

  // --- オフスクリーンレンダリングパス ---
  SceneManager::GetInstance()->DrawOffscreen();
  renderTexture_->PreDraw(commandList);
  // シーンマネージャに現在のシーンを描画させる（オフスクリーンへ）
  SceneManager::GetInstance()->Draw();
  // オフスクリーン描画の終了処理
  renderTexture_->PostDraw(commandList);

  // --- メインバックバッファへのポストエフェクト描画 ---
  DX12Context::GetInstance()->SetBackBufferAsRenderTarget();
  SrvManager::GetInstance()->PreDraw();
  PostProcessManager::GetInstance()->Draw(renderTexture_.get());

  // ImGuiの描画
  imGuiManager_->Draw();

  // 描画後処理
  DX12Context::GetInstance()->PostDraw();
}