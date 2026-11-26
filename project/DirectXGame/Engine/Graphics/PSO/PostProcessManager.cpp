#include "PostProcessManager.h"

#include "RenderTexture.h"
#include "DX12Context.h"
#include "SrvManager.h"
#include "PipelineManager.h"
#include "TextureManager.h"
#include "Object3d/Object3dCommon.h"
#include "Camera/Camera.h"
#include "Math/Functions/MathUtils.h"
#include <cstring>

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

void PostProcessManager::Initialize() {
    // PSO の生成
    postProcessPSO_ = PipelineManager::GetInstance()->CreatePostProcessPSO();
    colorFilterPSO_  = PipelineManager::GetInstance()->CreateColorFilterPSO();
    vignettePSO_     = PipelineManager::GetInstance()->CreateVignettePSO();
    smoothingPSO_    = PipelineManager::GetInstance()->CreateSmoothingPSO();
    gaussianBlurPSO_ = PipelineManager::GetInstance()->CreateGaussianBlurPSO();
    outlinePSO_      = PipelineManager::GetInstance()->CreateOutlinePSO();
    radialBlurPSO_   = PipelineManager::GetInstance()->CreateRadialBlurPSO();
    dissolvePSO_     = PipelineManager::GetInstance()->CreateDissolvePSO();
    randomPSO_       = PipelineManager::GetInstance()->CreateRandomPSO();
    hsvPSO_          = PipelineManager::GetInstance()->CreateHSVPSO();

    // 定数バッファの生成とマッピング
    paramsResource_ = DX12Context::GetInstance()->CreateBufferResource(sizeof(PostProcessParams));
    paramsResource_->Map(0, nullptr, reinterpret_cast<void**>(&paramsMapped_));

    // デフォルト値 (パススルー)
    paramsMapped_->colorScale[0] = 1.0f;
    paramsMapped_->colorScale[1] = 1.0f;
    paramsMapped_->colorScale[2] = 1.0f;
    paramsMapped_->strength = 1.0f;

    // ビネット用の定数バッファの生成とマッピング
    vignetteParamsResource_ = DX12Context::GetInstance()->CreateBufferResource(sizeof(VignetteParams));
    vignetteParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&vignetteParamsMapped_));
    vignetteParamsMapped_->scale = 16.0f;
    vignetteParamsMapped_->exponent = 0.8f;

    // 平滑化用の定数バッファの生成とマッピング
    smoothingParamsResource_ = DX12Context::GetInstance()->CreateBufferResource(sizeof(SmoothingParams));
    smoothingParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&smoothingParamsMapped_));
    smoothingParamsMapped_->kernelSize = 3;

    // ガウシアンフィルター用の定数バッファの生成とマッピング
    gaussianBlurParamsResource_ = DX12Context::GetInstance()->CreateBufferResource(sizeof(GaussianBlurParams));
    gaussianBlurParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&gaussianBlurParamsMapped_));
    gaussianBlurParamsMapped_->kernelSize = 3;
    gaussianBlurParamsMapped_->sigma = 2.0f;

    // アウトライン用の定数バッファの生成とマッピング
    outlineParamsResource_ = DX12Context::GetInstance()->CreateBufferResource(sizeof(OutlineParams));
    outlineParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&outlineParamsMapped_));
    outlineParamsMapped_->depthEdgeMultiplier = 6.0f;
    outlineParamsMapped_->colorEdgeMultiplier = 1.0f;
    std::memset(&currentProjectionInverse_, 0, sizeof(currentProjectionInverse_));

    // Radial Blur用の定数バッファの生成とマッピング
    radialBlurParamsResource_ = DX12Context::GetInstance()->CreateBufferResource(sizeof(RadialBlurParams));
    radialBlurParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&radialBlurParamsMapped_));
    radialBlurParamsMapped_->center[0] = 0.5f;
    radialBlurParamsMapped_->center[1] = 0.5f;
    radialBlurParamsMapped_->blurWidth = 0.01f;
    radialBlurParamsMapped_->sampleCount = 10;

    // Dissolve用の定数バッファの生成とマッピング
    dissolveParamsResource_ = DX12Context::GetInstance()->CreateBufferResource(sizeof(DissolveParams));
    dissolveParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveParamsMapped_));
    dissolveParamsMapped_->edgeColor[0] = 1.0f;
    dissolveParamsMapped_->edgeColor[1] = 0.4f;
    dissolveParamsMapped_->edgeColor[2] = 0.3f;
    dissolveParamsMapped_->threshold = 0.0f;
    dissolveParamsMapped_->edgeRange = 0.02f;

    // Random用の定数バッファの生成とマッピング
    randomParamsResource_ = DX12Context::GetInstance()->CreateBufferResource(sizeof(RandomParams));
    randomParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&randomParamsMapped_));
    randomParamsMapped_->time = 0.0f;
    randomParamsMapped_->strength = 0.0f;

    // HSV用の定数バッファの生成とマッピング
    hsvParamsResource_ = DX12Context::GetInstance()->CreateBufferResource(sizeof(HSVParams));
    hsvParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&hsvParamsMapped_));
    hsvParamsMapped_->hue = 0.0f;
    hsvParamsMapped_->saturation = 0.0f;
    hsvParamsMapped_->value = 0.0f;

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

    // 深度バッファのリソースバリア（DEPTH_WRITE -> PIXEL_SHADER_RESOURCE）
    bool transitionDepth = (currentMode_ == kModeOutline);
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

    // 各モードの描画処理呼び出し
    if (currentMode_ == kModeCopy) {
        DrawCopy(commandList);
    } else if (currentMode_ == kModeVignette) {
        DrawVignette(commandList, vignetteScaleNext_, vignetteExponentNext_);
    } else if (currentMode_ == kModeSmoothing) {
        DrawSmoothing(commandList, smoothingKernelSizeNext_);
    } else if (currentMode_ == kModeGaussianBlur) {
        DrawGaussianBlur(commandList, gaussianBlurKernelSizeNext_, gaussianBlurSigmaNext_);
    } else if (currentMode_ == kModeOutline) {
        DrawOutline(commandList, outlineDepthEdgeMultiplierNext_, outlineColorEdgeMultiplierNext_);
    } else if (currentMode_ == kModeRadialBlur) {
        DrawRadialBlur(commandList, radialBlurCenterXNext_, radialBlurCenterYNext_, radialBlurWidthNext_, radialBlurSampleCountNext_);
    } else if (currentMode_ == kModeDissolve) {
        float edgeColor[3] = { dissolveEdgeColorNext_[0], dissolveEdgeColorNext_[1], dissolveEdgeColorNext_[2] };
        DrawDissolve(commandList, dissolveThresholdNext_, dissolveEdgeRangeNext_, edgeColor, dissolveMaskPathNext_);
    } else if (currentMode_ == kModeRandom) {
        DrawRandom(commandList, randomStrengthNext_);
    } else if (currentMode_ == kModeHSV) {
        DrawHSV(commandList, hsvHueNext_, hsvSaturationNext_, hsvValueNext_);
    } else {
        DrawColorFilter(commandList, strengthNext_);
    }

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
    // Randomエフェクト用の時間を更新
    if (modeNext_ == kModeRandom) {
        currentRandomTime_ += deltaTime;
    }
}

void PostProcessManager::DrawCopy(ID3D12GraphicsCommandList* commandList) {
    commandList->SetPipelineState(postProcessPSO_.Get());
    if (currentColorFilterMode_ != currentMode_) {
        currentColorFilterMode_ = currentMode_;
        paramsMapped_->colorScale[0] = 1.0f;
        paramsMapped_->colorScale[1] = 1.0f;
        paramsMapped_->colorScale[2] = 1.0f;
        paramsMapped_->strength = 1.0f;
    }
    commandList->SetGraphicsRootConstantBufferView(0, paramsResource_->GetGPUVirtualAddress());
}

void PostProcessManager::DrawVignette(ID3D12GraphicsCommandList* commandList, float scale, float exponent) {
    commandList->SetPipelineState(vignettePSO_.Get());
    if (currentVignetteScale_ != scale || currentVignetteExponent_ != exponent) {
        currentVignetteScale_ = scale;
        currentVignetteExponent_ = exponent;
        vignetteParamsMapped_->scale = scale;
        vignetteParamsMapped_->exponent = exponent;
    }
    commandList->SetGraphicsRootConstantBufferView(0, vignetteParamsResource_->GetGPUVirtualAddress());
}

void PostProcessManager::DrawSmoothing(ID3D12GraphicsCommandList* commandList, int kernelSize) {
    commandList->SetPipelineState(smoothingPSO_.Get());
    if (currentSmoothingKernelSize_ != kernelSize) {
        currentSmoothingKernelSize_ = kernelSize;
        smoothingParamsMapped_->kernelSize = kernelSize;
    }
    commandList->SetGraphicsRootConstantBufferView(0, smoothingParamsResource_->GetGPUVirtualAddress());
}

void PostProcessManager::DrawGaussianBlur(ID3D12GraphicsCommandList* commandList, int kernelSize, float sigma) {
    commandList->SetPipelineState(gaussianBlurPSO_.Get());
    if (currentGaussianBlurKernelSize_ != kernelSize || currentGaussianBlurSigma_ != sigma) {
        currentGaussianBlurKernelSize_ = kernelSize;
        currentGaussianBlurSigma_ = sigma;
        gaussianBlurParamsMapped_->kernelSize = kernelSize;
        gaussianBlurParamsMapped_->sigma = sigma;
    }
    commandList->SetGraphicsRootConstantBufferView(0, gaussianBlurParamsResource_->GetGPUVirtualAddress());
}

void PostProcessManager::DrawOutline(ID3D12GraphicsCommandList* commandList, float depthEdgeMultiplier, float colorEdgeMultiplier) {
    commandList->SetPipelineState(outlinePSO_.Get());
    Camera* camera = Object3dCommon::GetInstance()->GetDefaultCamera();
    if (camera) {
        Matrix4x4 proj = camera->GetProjectionMatrix();
        Matrix4x4 projInv = MathUtils::Inverse(proj);
        if (std::memcmp(&currentProjectionInverse_, &projInv, sizeof(Matrix4x4)) != 0 || 
            currentOutlineDepthEdgeMultiplier_ != depthEdgeMultiplier ||
            currentOutlineColorEdgeMultiplier_ != colorEdgeMultiplier) {
            
            currentProjectionInverse_ = projInv;
            currentOutlineDepthEdgeMultiplier_ = depthEdgeMultiplier;
            currentOutlineColorEdgeMultiplier_ = colorEdgeMultiplier;
            outlineParamsMapped_->projectionInverse = projInv;
            outlineParamsMapped_->depthEdgeMultiplier = depthEdgeMultiplier;
            outlineParamsMapped_->colorEdgeMultiplier = colorEdgeMultiplier;
        }
    }
    commandList->SetGraphicsRootConstantBufferView(0, outlineParamsResource_->GetGPUVirtualAddress());
}

void PostProcessManager::DrawRadialBlur(ID3D12GraphicsCommandList* commandList, float centerX, float centerY, float width, int sampleCount) {
    commandList->SetPipelineState(radialBlurPSO_.Get());
    if (currentRadialBlurCenterX_ != centerX ||
        currentRadialBlurCenterY_ != centerY ||
        currentRadialBlurWidth_ != width ||
        currentRadialBlurSampleCount_ != sampleCount) {
        
        currentRadialBlurCenterX_ = centerX;
        currentRadialBlurCenterY_ = centerY;
        currentRadialBlurWidth_ = width;
        currentRadialBlurSampleCount_ = sampleCount;
        
        radialBlurParamsMapped_->center[0] = centerX;
        radialBlurParamsMapped_->center[1] = centerY;
        radialBlurParamsMapped_->blurWidth = width;
        radialBlurParamsMapped_->sampleCount = sampleCount;
    }
    commandList->SetGraphicsRootConstantBufferView(0, radialBlurParamsResource_->GetGPUVirtualAddress());
}

void PostProcessManager::DrawDissolve(ID3D12GraphicsCommandList* commandList, float threshold, float edgeRange, const float edgeColor[3], const std::string& maskPath) {
    commandList->SetPipelineState(dissolvePSO_.Get());
    
    // t1 (Root Parameter 2) にマスクテクスチャの SRV をバインド
    uint32_t maskSrvIndex = TextureManager::GetInstance()->GetSrvIndex(maskPath);
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, maskSrvIndex);
    
    if (currentDissolveEdgeColor_[0] != edgeColor[0] ||
        currentDissolveEdgeColor_[1] != edgeColor[1] ||
        currentDissolveEdgeColor_[2] != edgeColor[2] ||
        currentDissolveThreshold_ != threshold ||
        currentDissolveEdgeRange_ != edgeRange ||
        currentDissolveMaskPath_ != maskPath) {
        
        currentDissolveEdgeColor_[0] = edgeColor[0];
        currentDissolveEdgeColor_[1] = edgeColor[1];
        currentDissolveEdgeColor_[2] = edgeColor[2];
        currentDissolveThreshold_ = threshold;
        currentDissolveEdgeRange_ = edgeRange;
        currentDissolveMaskPath_ = maskPath;
        
        dissolveParamsMapped_->edgeColor[0] = edgeColor[0];
        dissolveParamsMapped_->edgeColor[1] = edgeColor[1];
        dissolveParamsMapped_->edgeColor[2] = edgeColor[2];
        dissolveParamsMapped_->threshold = threshold;
        dissolveParamsMapped_->edgeRange = edgeRange;
    }
    commandList->SetGraphicsRootConstantBufferView(0, dissolveParamsResource_->GetGPUVirtualAddress());
}

void PostProcessManager::DrawRandom(ID3D12GraphicsCommandList* commandList, float strength) {
    commandList->SetPipelineState(randomPSO_.Get());

    // 時間経過を伴うため毎フレーム定数バッファを更新する
    randomParamsMapped_->time = currentRandomTime_;
    randomParamsMapped_->strength = strength;

    commandList->SetGraphicsRootConstantBufferView(0, randomParamsResource_->GetGPUVirtualAddress());
}

void PostProcessManager::DrawColorFilter(ID3D12GraphicsCommandList* commandList, float strength) {
    commandList->SetPipelineState(colorFilterPSO_.Get());

    if (currentColorFilterMode_ != currentMode_ || currentStrength_ != strength) {
        currentColorFilterMode_ = currentMode_;
        currentStrength_ = strength;

        if (currentMode_ == kModeGrayscale) {
            paramsMapped_->colorScale[0] = 1.0f;
            paramsMapped_->colorScale[1] = 1.0f;
            paramsMapped_->colorScale[2] = 1.0f;
        } else { // kModeSepia
            // RGB (112, 74, 43) を 112 で正規化
            paramsMapped_->colorScale[0] = 1.0f;
            paramsMapped_->colorScale[1] = 74.0f / 112.0f;
            paramsMapped_->colorScale[2] = 43.0f / 112.0f;
        }
        paramsMapped_->strength = strength;
    }
    commandList->SetGraphicsRootConstantBufferView(0, paramsResource_->GetGPUVirtualAddress());
}

void PostProcessManager::DrawHSV(ID3D12GraphicsCommandList* commandList, float hue, float saturation, float value) {
    commandList->SetPipelineState(hsvPSO_.Get());

    if (currentHSVHue_ != hue || currentHSVSaturation_ != saturation || currentHSVValue_ != value) {
        currentHSVHue_ = hue;
        currentHSVSaturation_ = saturation;
        currentHSVValue_ = value;

        hsvParamsMapped_->hue = hue;
        hsvParamsMapped_->saturation = saturation;
        hsvParamsMapped_->value = value;
    }
    commandList->SetGraphicsRootConstantBufferView(0, hsvParamsResource_->GetGPUVirtualAddress());
}
