#include "PostEffects.h"

#include "DX12Context.h"
#include "PipelineManager.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "Object3d/Object3dCommon.h"
#include "Camera/Camera.h"
#include "Math/Functions/MathUtils.h"
#include <cstring>

namespace {
template <typename T>
void CreateMappedConstantBuffer(Microsoft::WRL::ComPtr<ID3D12Resource>& resource, T*& mapped) {
    resource = DX12Context::GetInstance()->CreateBufferResource(sizeof(T));
    resource->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
}
} // namespace

// =========================================================================
// CopyPostEffect
// =========================================================================
void CopyPostEffect::Initialize() {
    pso_ = PipelineManager::GetInstance()->CreatePostProcessPSO();
    CreateMappedConstantBuffer(paramsResource_, paramsMapped_);
    paramsMapped_->colorScale[0] = 1.0f;
    paramsMapped_->colorScale[1] = 1.0f;
    paramsMapped_->colorScale[2] = 1.0f;
    paramsMapped_->strength = 1.0f;
}

void CopyPostEffect::Apply(ID3D12GraphicsCommandList* commandList) {
    commandList->SetPipelineState(pso_.Get());
    commandList->SetGraphicsRootConstantBufferView(0, paramsResource_->GetGPUVirtualAddress());
}

// =========================================================================
// ColorFilterPostEffect
// =========================================================================
ColorFilterPostEffect::ColorFilterPostEffect(bool isSepia)
    : isSepia_(isSepia) {
}

void ColorFilterPostEffect::Initialize() {
    pso_ = PipelineManager::GetInstance()->CreateColorFilterPSO();
    CreateMappedConstantBuffer(paramsResource_, paramsMapped_);
    if (isSepia_) {
        // RGB (112, 74, 43) を 112 で正規化
        paramsMapped_->colorScale[0] = 1.0f;
        paramsMapped_->colorScale[1] = 74.0f / 112.0f;
        paramsMapped_->colorScale[2] = 43.0f / 112.0f;
    } else {
        paramsMapped_->colorScale[0] = 1.0f;
        paramsMapped_->colorScale[1] = 1.0f;
        paramsMapped_->colorScale[2] = 1.0f;
    }
    paramsMapped_->strength = strengthNext_;
    currentStrength_ = strengthNext_;
}

void ColorFilterPostEffect::Apply(ID3D12GraphicsCommandList* commandList) {
    commandList->SetPipelineState(pso_.Get());
    if (currentStrength_ != strengthNext_) {
        currentStrength_ = strengthNext_;
        paramsMapped_->strength = strengthNext_;
    }
    commandList->SetGraphicsRootConstantBufferView(0, paramsResource_->GetGPUVirtualAddress());
}

// =========================================================================
// VignettePostEffect
// =========================================================================
void VignettePostEffect::Initialize() {
    pso_ = PipelineManager::GetInstance()->CreateVignettePSO();
    CreateMappedConstantBuffer(paramsResource_, paramsMapped_);
    paramsMapped_->scale = scaleNext_;
    paramsMapped_->exponent = exponentNext_;
}

void VignettePostEffect::Apply(ID3D12GraphicsCommandList* commandList) {
    commandList->SetPipelineState(pso_.Get());
    if (currentScale_ != scaleNext_ || currentExponent_ != exponentNext_) {
        currentScale_ = scaleNext_;
        currentExponent_ = exponentNext_;
        paramsMapped_->scale = scaleNext_;
        paramsMapped_->exponent = exponentNext_;
    }
    commandList->SetGraphicsRootConstantBufferView(0, paramsResource_->GetGPUVirtualAddress());
}

// =========================================================================
// SmoothingPostEffect
// =========================================================================
void SmoothingPostEffect::Initialize() {
    pso_ = PipelineManager::GetInstance()->CreateSmoothingPSO();
    CreateMappedConstantBuffer(paramsResource_, paramsMapped_);
    paramsMapped_->kernelSize = kernelSizeNext_;
}

void SmoothingPostEffect::Apply(ID3D12GraphicsCommandList* commandList) {
    commandList->SetPipelineState(pso_.Get());
    if (currentKernelSize_ != kernelSizeNext_) {
        currentKernelSize_ = kernelSizeNext_;
        paramsMapped_->kernelSize = kernelSizeNext_;
    }
    commandList->SetGraphicsRootConstantBufferView(0, paramsResource_->GetGPUVirtualAddress());
}

// =========================================================================
// GaussianBlurPostEffect
// =========================================================================
void GaussianBlurPostEffect::Initialize() {
    pso_ = PipelineManager::GetInstance()->CreateGaussianBlurPSO();
    CreateMappedConstantBuffer(paramsResource_, paramsMapped_);
    paramsMapped_->kernelSize = kernelSizeNext_;
    paramsMapped_->sigma = sigmaNext_;
}

void GaussianBlurPostEffect::Apply(ID3D12GraphicsCommandList* commandList) {
    commandList->SetPipelineState(pso_.Get());
    if (currentKernelSize_ != kernelSizeNext_ || currentSigma_ != sigmaNext_) {
        currentKernelSize_ = kernelSizeNext_;
        currentSigma_ = sigmaNext_;
        paramsMapped_->kernelSize = kernelSizeNext_;
        paramsMapped_->sigma = sigmaNext_;
    }
    commandList->SetGraphicsRootConstantBufferView(0, paramsResource_->GetGPUVirtualAddress());
}

// =========================================================================
// OutlinePostEffect
// =========================================================================
void OutlinePostEffect::Initialize() {
    pso_ = PipelineManager::GetInstance()->CreateOutlinePSO();
    CreateMappedConstantBuffer(paramsResource_, paramsMapped_);
    paramsMapped_->depthEdgeMultiplier = depthEdgeMultiplierNext_;
    paramsMapped_->colorEdgeMultiplier = colorEdgeMultiplierNext_;
    std::memset(&currentProjectionInverse_, 0, sizeof(currentProjectionInverse_));
}

void OutlinePostEffect::Apply(ID3D12GraphicsCommandList* commandList) {
    commandList->SetPipelineState(pso_.Get());
    Camera* camera = Object3dCommon::GetInstance()->GetDefaultCamera();
    if (camera) {
        Matrix4x4 proj = camera->GetProjectionMatrix();
        Matrix4x4 projInv = MathUtils::Inverse(proj);
        if (std::memcmp(&currentProjectionInverse_, &projInv, sizeof(Matrix4x4)) != 0 ||
            currentDepthEdgeMultiplier_ != depthEdgeMultiplierNext_ ||
            currentColorEdgeMultiplier_ != colorEdgeMultiplierNext_) {

            currentProjectionInverse_ = projInv;
            currentDepthEdgeMultiplier_ = depthEdgeMultiplierNext_;
            currentColorEdgeMultiplier_ = colorEdgeMultiplierNext_;
            paramsMapped_->projectionInverse = projInv;
            paramsMapped_->depthEdgeMultiplier = depthEdgeMultiplierNext_;
            paramsMapped_->colorEdgeMultiplier = colorEdgeMultiplierNext_;
        }
    }
    commandList->SetGraphicsRootConstantBufferView(0, paramsResource_->GetGPUVirtualAddress());
}

// =========================================================================
// RadialBlurPostEffect
// =========================================================================
void RadialBlurPostEffect::Initialize() {
    pso_ = PipelineManager::GetInstance()->CreateRadialBlurPSO();
    CreateMappedConstantBuffer(paramsResource_, paramsMapped_);
    paramsMapped_->center[0] = centerXNext_;
    paramsMapped_->center[1] = centerYNext_;
    paramsMapped_->blurWidth = widthNext_;
    paramsMapped_->sampleCount = sampleCountNext_;
}

void RadialBlurPostEffect::Apply(ID3D12GraphicsCommandList* commandList) {
    commandList->SetPipelineState(pso_.Get());
    if (currentCenterX_ != centerXNext_ ||
        currentCenterY_ != centerYNext_ ||
        currentWidth_ != widthNext_ ||
        currentSampleCount_ != sampleCountNext_) {

        currentCenterX_ = centerXNext_;
        currentCenterY_ = centerYNext_;
        currentWidth_ = widthNext_;
        currentSampleCount_ = sampleCountNext_;

        paramsMapped_->center[0] = centerXNext_;
        paramsMapped_->center[1] = centerYNext_;
        paramsMapped_->blurWidth = widthNext_;
        paramsMapped_->sampleCount = sampleCountNext_;
    }
    commandList->SetGraphicsRootConstantBufferView(0, paramsResource_->GetGPUVirtualAddress());
}

// =========================================================================
// DissolvePostEffect
// =========================================================================
void DissolvePostEffect::Initialize() {
    pso_ = PipelineManager::GetInstance()->CreateDissolvePSO();
    CreateMappedConstantBuffer(paramsResource_, paramsMapped_);
    paramsMapped_->edgeColor[0] = edgeColorNext_[0];
    paramsMapped_->edgeColor[1] = edgeColorNext_[1];
    paramsMapped_->edgeColor[2] = edgeColorNext_[2];
    paramsMapped_->threshold = thresholdNext_;
    paramsMapped_->edgeRange = edgeRangeNext_;
}

void DissolvePostEffect::Apply(ID3D12GraphicsCommandList* commandList) {
    commandList->SetPipelineState(pso_.Get());

    // t1 (Root Parameter 2) にマスクテクスチャの SRV をバインド
    uint32_t maskSrvIndex = TextureManager::GetInstance()->GetSrvIndex(maskPathNext_);
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, maskSrvIndex);

    if (currentEdgeColor_[0] != edgeColorNext_[0] ||
        currentEdgeColor_[1] != edgeColorNext_[1] ||
        currentEdgeColor_[2] != edgeColorNext_[2] ||
        currentThreshold_ != thresholdNext_ ||
        currentEdgeRange_ != edgeRangeNext_ ||
        currentMaskPath_ != maskPathNext_) {

        currentEdgeColor_[0] = edgeColorNext_[0];
        currentEdgeColor_[1] = edgeColorNext_[1];
        currentEdgeColor_[2] = edgeColorNext_[2];
        currentThreshold_ = thresholdNext_;
        currentEdgeRange_ = edgeRangeNext_;
        currentMaskPath_ = maskPathNext_;

        paramsMapped_->edgeColor[0] = edgeColorNext_[0];
        paramsMapped_->edgeColor[1] = edgeColorNext_[1];
        paramsMapped_->edgeColor[2] = edgeColorNext_[2];
        paramsMapped_->threshold = thresholdNext_;
        paramsMapped_->edgeRange = edgeRangeNext_;
    }
    commandList->SetGraphicsRootConstantBufferView(0, paramsResource_->GetGPUVirtualAddress());
}

// =========================================================================
// RandomPostEffect
// =========================================================================
void RandomPostEffect::Initialize() {
    pso_ = PipelineManager::GetInstance()->CreateRandomPSO();
    CreateMappedConstantBuffer(paramsResource_, paramsMapped_);
    paramsMapped_->time = 0.0f;
    paramsMapped_->strength = strengthNext_;
}

void RandomPostEffect::Update(float deltaTime) {
    currentTime_ += deltaTime;
}

void RandomPostEffect::Apply(ID3D12GraphicsCommandList* commandList) {
    commandList->SetPipelineState(pso_.Get());
    // 時間経過を伴うため毎フレーム定数バッファを更新する
    paramsMapped_->time = currentTime_;
    paramsMapped_->strength = strengthNext_;
    commandList->SetGraphicsRootConstantBufferView(0, paramsResource_->GetGPUVirtualAddress());
}

// =========================================================================
// HSVPostEffect
// =========================================================================
void HSVPostEffect::Initialize() {
    pso_ = PipelineManager::GetInstance()->CreateHSVPSO();
    CreateMappedConstantBuffer(paramsResource_, paramsMapped_);
    paramsMapped_->hue = hueNext_;
    paramsMapped_->saturation = saturationNext_;
    paramsMapped_->value = valueNext_;
}

void HSVPostEffect::Apply(ID3D12GraphicsCommandList* commandList) {
    commandList->SetPipelineState(pso_.Get());
    if (currentHue_ != hueNext_ || currentSaturation_ != saturationNext_ || currentValue_ != valueNext_) {
        currentHue_ = hueNext_;
        currentSaturation_ = saturationNext_;
        currentValue_ = valueNext_;

        paramsMapped_->hue = hueNext_;
        paramsMapped_->saturation = saturationNext_;
        paramsMapped_->value = valueNext_;
    }
    commandList->SetGraphicsRootConstantBufferView(0, paramsResource_->GetGPUVirtualAddress());
}
