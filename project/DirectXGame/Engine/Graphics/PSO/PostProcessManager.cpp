#include "PostProcessManager.h"

#include "RenderTexture.h"
#include "DX12Context.h"
#include "SrvManager.h"
#include "PipelineManager.h"
#include "TextureManager.h"

// 静的メンバの実体化
std::unique_ptr<PostProcessManager> PostProcessManager::instance_;

PostProcessManager* PostProcessManager::GetInstance() {
    if (!instance_) {
        instance_ = std::make_unique<PostProcessManager>(Token{});
    }
    return instance_.get();
}

void PostProcessManager::Destroy() {
    instance_.reset();
}

IPostEffect* PostProcessManager::FindEffect(int mode) const {
    auto it = effects_.find(mode);
    if (it != effects_.end()) {
        return it->second.get();
    }
    auto copyIt = effects_.find(kModeCopy);
    return (copyIt != effects_.end()) ? copyIt->second.get() : nullptr;
}

void PostProcessManager::Initialize() {
    effects_.clear();

    RegisterEffect<CopyPostEffect>(kModeCopy);
    grayscaleEffect_ = RegisterEffect<ColorFilterPostEffect>(kModeGrayscale, false);
    sepiaEffect_ = RegisterEffect<ColorFilterPostEffect>(kModeSepia, true);
    vignetteEffect_ = RegisterEffect<VignettePostEffect>(kModeVignette);
    smoothingEffect_ = RegisterEffect<SmoothingPostEffect>(kModeSmoothing);
    gaussianBlurEffect_ = RegisterEffect<GaussianBlurPostEffect>(kModeGaussianBlur);
    outlineEffect_ = RegisterEffect<OutlinePostEffect>(kModeOutline);
    radialBlurEffect_ = RegisterEffect<RadialBlurPostEffect>(kModeRadialBlur);
    dissolveEffect_ = RegisterEffect<DissolvePostEffect>(kModeDissolve);
    randomEffect_ = RegisterEffect<RandomPostEffect>(kModeRandom);
    hsvEffect_ = RegisterEffect<HSVPostEffect>(kModeHSV);

    // マスクテクスチャを事前にロード
    TextureManager::GetInstance()->LoadTexture("masks/noise0.png");
    TextureManager::GetInstance()->LoadTexture("masks/noise1.png");

    // 深度バッファ用の SRV を作成
    depthSrvIndex_ = SrvManager::GetInstance()->Allocate();
    SrvManager::GetInstance()->CreateSRVForTexture(
        depthSrvIndex_,
        DX12Context::GetInstance()->GetDepthStencilResource(),
        DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
        1,
        false
    );
}

void PostProcessManager::Draw(RenderTexture* renderTexture) {
    ID3D12GraphicsCommandList* commandList = DX12Context::GetInstance()->GetCommandList();

    /// モードとパラメータを遅延適用
    currentMode_ = modeNext_;

    IPostEffect* activeEffect = FindEffect(currentMode_);
    if (!activeEffect) {
        return;
    }

    // 深度バッファのリソースバリア（DEPTH_WRITE -> PIXEL_SHADER_RESOURCE）
    bool transitionDepth = activeEffect->NeedsDepthResource();
    if (transitionDepth) {
        ID3D12Resource* depthResource = DX12Context::GetInstance()->GetDepthStencilResource();
        D3D12_RESOURCE_BARRIER barrierDepth{};
        barrierDepth.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrierDepth.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrierDepth.Transition.pResource = depthResource;
        barrierDepth.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barrierDepth.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        commandList->ResourceBarrier(1, &barrierDepth);
    }

    // 共通ルートシグネチャのセット
    commandList->SetGraphicsRootSignature(
        PipelineManager::GetInstance()->GetPostProcessRootSignature());

    // オフスクリーン描画結果を t0 にセット
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(1, renderTexture->GetSrvIndex());
    // 深度描画結果を t1 にセット
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, depthSrvIndex_);

    // 選択されたエフェクトの適用（ポリモーフィズム）
    activeEffect->Apply(commandList);

    // 全画面三角形の描画
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);

    // 深度バッファのリソースバリア（PIXEL_SHADER_RESOURCE -> DEPTH_WRITE）
    if (transitionDepth) {
        ID3D12Resource* depthResource = DX12Context::GetInstance()->GetDepthStencilResource();
        D3D12_RESOURCE_BARRIER barrierDepth{};
        barrierDepth.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrierDepth.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrierDepth.Transition.pResource = depthResource;
        barrierDepth.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrierDepth.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        commandList->ResourceBarrier(1, &barrierDepth);
    }
}

void PostProcessManager::Update(float deltaTime) {
    if (IPostEffect* activeEffect = FindEffect(modeNext_)) {
        activeEffect->Update(deltaTime);
    }
}
