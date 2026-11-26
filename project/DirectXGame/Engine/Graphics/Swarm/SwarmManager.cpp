#include "SwarmManager.h"
#include "Base/DX12Context.h"
#include "Base/SrvManager.h"
#include "PSO/PipelineManager.h"
#include "Texture/TextureManager.h"
#include "Logger/Logger.h"
#include "MathUtils.h"

#include <random>
#include <algorithm>
#include <cmath>
#include <cassert>

using namespace Microsoft::WRL;

// =========================================================================
// ボス形態フェーズ State Pattern 実装
// =========================================================================
float SwarmManager::ISwarmBossPhaseState::GetTargetCameraYaw(const SwarmManager& manager) const {
    const float kSectionYawStep = 0.48f;
    return manager.bossBaseYaw_ + static_cast<float>(manager.currentSection_) * kSectionYawStep;
}

float SwarmManager::ISwarmBossPhaseState::GetTargetCameraPitch(const SwarmManager& manager) const {
    (void)manager;
    return -0.18f;
}

class SwarmManager::BossPhaseFinalState final : public SwarmManager::ISwarmBossPhaseState {
public:
    int GetPhaseNumber() const override { return 3; }

    void OnEnter(SwarmManager& manager, const Vector3& playerPos) override {
        manager.chargeGroups_.clear();
        manager.TriggerFinalChargeGroup(playerPos);
    }

    void CheckTransition(SwarmManager& manager, const Vector3& playerPos) override {
        (void)manager;
        (void)playerPos;
    }

    void Update(SwarmManager& manager, float deltaTime, const Vector3& playerPos) override {
        (void)deltaTime;
        if (manager.chargeGroups_.empty()) {
            // 最終突撃がカバーされて帰還中の場合、全生存機が帰還完了したら再度最終突撃を開始
            bool anyReturning = false;
            bool hasRemaining = false;
            for (uint32_t i = 0; i < manager.droneCount_; ++i) {
                const auto& d = manager.dronesShadow_[i];
                if (d.hp > 0.0f && d.state != 2) {
                    if (d.state == 4) {
                        anyReturning = true;
                        break;
                    }
                    hasRemaining = true;
                }
            }
            if (!anyReturning && hasRemaining) {
                manager.TriggerFinalChargeGroup(playerPos);
            }
        }
    }

    bool CanTriggerNormalCharge() const override { return false; }

    void ExecuteCharge(SwarmManager& manager, const std::vector<uint32_t>& selectedIndices, const Vector3& playerPos) const override {
        (void)manager;
        (void)selectedIndices;
        (void)playerPos;
    }

    bool ShouldUpdateCameraSection() const override { return false; }
    float ComputeAnchorLateral(float bossMoveTimer) const override {
        (void)bossMoveTimer;
        return 0.0f;
    }

    float GetTargetCameraYaw(const SwarmManager& manager) const override {
        if (!manager.chargeGroups_.empty()) {
            Vector3 diff = MathUtils::Subtract(manager.chargeGroups_[0].centerPos, manager.bossPlayerBasePos_);
            float distXZ = std::sqrt(diff.x * diff.x + diff.z * diff.z);
            if (distXZ > 2.5f) {
                Vector3 forward = { std::sin(manager.bossBaseYaw_), 0.0f, std::cos(manager.bossBaseYaw_) };
                Vector3 right = { std::cos(manager.bossBaseYaw_), 0.0f, -std::sin(manager.bossBaseYaw_) };
                float localX = MathUtils::Dot(diff, right);
                float localZ = MathUtils::Dot(diff, forward);
                float relYaw = std::atan2(localX, (std::max)(0.1f, localZ));
                return manager.bossBaseYaw_ + std::clamp(relYaw, -0.65f, 0.65f);
            }
        }
        return manager.bossBaseYaw_;
    }

    float GetTargetCameraPitch(const SwarmManager& manager) const override {
        if (!manager.chargeGroups_.empty()) {
            Vector3 diff = MathUtils::Subtract(manager.chargeGroups_[0].centerPos, manager.bossPlayerBasePos_);
            float dist = MathUtils::Length(diff);
            if (dist > 1.0f) {
                return -std::clamp(std::atan2(diff.y, dist), -0.35f, 0.35f);
            }
        }
        return -0.18f;
    }
};

class SwarmManager::BossPhase2State final : public SwarmManager::ISwarmBossPhaseState {
public:
    int GetPhaseNumber() const override { return 2; }

    void CheckTransition(SwarmManager& manager, const Vector3& playerPos) override {
        uint32_t aliveCount = manager.GetAliveDroneCount();
        if (aliveCount <= 15 && aliveCount > 0) {
            manager.ChangeBossPhaseState(std::make_unique<BossPhaseFinalState>(), playerPos);
        }
    }

    void Update(SwarmManager& manager, float deltaTime, const Vector3& playerPos) override {
        CheckTransition(manager, playerPos);
        if (manager.bossPhaseState_.get() != this) {
            return;
        }
        manager.noShootTimer_ += deltaTime;
        manager.periodicAttackTimer_ += deltaTime;

        if (manager.noShootTimer_ >= 2.0f) {
            manager.noShootTimer_ = 0.0f;
            manager.TriggerCharge(3, playerPos);
        }
        if (manager.periodicAttackTimer_ >= 10.0f) {
            manager.periodicAttackTimer_ -= 10.0f;
            manager.TriggerCharge(10, playerPos);
        }
    }

    void ExecuteCharge(SwarmManager& manager, const std::vector<uint32_t>& selectedIndices, const Vector3& playerPos) const override {
        manager.CreateChargeGroup(selectedIndices, false, playerPos);
    }
};

class SwarmManager::BossPhase1State final : public SwarmManager::ISwarmBossPhaseState {
public:
    int GetPhaseNumber() const override { return 1; }

    void CheckTransition(SwarmManager& manager, const Vector3& playerPos) override {
        uint32_t aliveCount = manager.GetAliveDroneCount();
        if (aliveCount <= 15 && aliveCount > 0) {
            manager.ChangeBossPhaseState(std::make_unique<BossPhaseFinalState>(), playerPos);
        } else if (aliveCount <= manager.bossMaxHp_ / 2) {
            manager.ChangeBossPhaseState(std::make_unique<BossPhase2State>(), playerPos);
        }
    }

    void Update(SwarmManager& manager, float deltaTime, const Vector3& playerPos) override {
        CheckTransition(manager, playerPos);
        if (manager.bossPhaseState_.get() != this) {
            return;
        }
        manager.noShootTimer_ += deltaTime;
        manager.periodicAttackTimer_ += deltaTime;

        if (manager.noShootTimer_ >= 2.0f) {
            manager.noShootTimer_ = 0.0f;
            manager.TriggerCharge(3, playerPos);
        }
        if (manager.periodicAttackTimer_ >= 10.0f) {
            manager.periodicAttackTimer_ -= 10.0f;
            manager.TriggerCharge(10, playerPos);
        }
    }

    void ExecuteCharge(SwarmManager& manager, const std::vector<uint32_t>& selectedIndices, const Vector3& playerPos) const override {
        (void)playerPos;
        for (size_t i = 0; i < selectedIndices.size(); ++i) {
            uint32_t idx = selectedIndices[i];
            manager.dronesShadow_[idx].state = 1; // Charge
            manager.dronesShadow_[idx].stateTimer = -static_cast<float>(i) * 0.18f;
            manager.frameEvents_.chargeStartPositions.push_back(manager.dronesShadow_[idx].position);
        }
    }
};

void SwarmManager::ChangeBossPhaseState(std::unique_ptr<ISwarmBossPhaseState> nextState, const Vector3& playerPos) {
    bossPhaseState_ = std::move(nextState);
    if (bossPhaseState_) {
        bossPhaseState_->OnEnter(*this, playerPos);
    }
}

void SwarmManager::TriggerFinalChargeGroup(const Vector3& playerPos) {
    std::vector<uint32_t> remainingIndices;
    for (uint32_t i = 0; i < droneCount_; ++i) {
        if (dronesShadow_[i].hp > 0.0f && dronesShadow_[i].state != 2) {
            remainingIndices.push_back(i);
        }
    }
    if (!remainingIndices.empty()) {
        CreateChargeGroup(remainingIndices, true, playerPos);
    }
}

void SwarmManager::Initialize(uint32_t droneCount,
                              const std::string& modelDirectory,
                              const std::string& modelFilename) {
    droneCount_ = droneCount;

    // デフォルトシミュレーションパラメータ
    settings_.maxSpeed = 16.0f;
    settings_.neighborRadius = 12.0f;
    settings_.separationDist = 3.5f;
    settings_.boundsMin = { -60.0f, -5.0f, 10.0f };
    settings_.boundsMax = { 60.0f, 40.0f, 160.0f };
    settings_.separationWeight = 2.2f;
    settings_.alignmentWeight = 1.0f;
    settings_.cohesionWeight = 1.1f;
    settings_.targetWeight = 0.9f;
    settings_.attackDistance = 30.0f;
    settings_.ChargeSpeed = 18.0f;
    settings_.droneCount = droneCount_;
    settings_.rayRadius = 2.0f;
    settings_.attackDamage = 0.0f;

    TextureManager::GetInstance()->LoadTexture("white.png");
    TextureManager::GetInstance()->LoadTexture("ui/enemyHpFrame.png");

    InitializeBuffers();
    InitializePipeline();
    InitializeModel(modelDirectory, modelFilename);
}

void SwarmManager::InitializeBuffers() {
    auto dxContext = DX12Context::GetInstance();
    auto srvManager = SrvManager::GetInstance();

    size_t bufferSize = sizeof(Swarm::DroneData) * droneCount_;

    // 1. Ping-Pongバッファの生成 (GPU DEFAULT Heap)
    droneBuffers_[0] = dxContext->CreateUAVBufferResource(bufferSize);
    droneBuffers_[1] = dxContext->CreateUAVBufferResource(bufferSize);

    // 2. 初期ドローンデータの作成 (開始時は非表示・非アクティブ)
    std::vector<Swarm::DroneData> initialData(droneCount_);
    for (uint32_t i = 0; i < droneCount_; ++i) {
        initialData[i].position = { 0.0f, -999.0f, 0.0f };
        initialData[i].velocity = { 0.0f, 0.0f, 0.0f };
        initialData[i].hp = 0.0f;
        initialData[i].state = 2; // Dead/非表示
        initialData[i].target = { 0.0f, 0.0f, 0.0f };
        initialData[i].stateTimer = 0.0f;
    }
    isActive_ = false;

    dronesShadow_ = initialData;
    crashSleeping_.assign(droneCount_, false);

    // 3. アップロード用バッファを経由してBuffer 0 & 1の両方に転送
    intermediateUploadBuffer_ = dxContext->CreateBufferResource(bufferSize);
    void* mappedPtr = nullptr;
    intermediateUploadBuffer_->Map(0, nullptr, &mappedPtr);
    std::memcpy(mappedPtr, initialData.data(), bufferSize);
    intermediateUploadBuffer_->Unmap(0, nullptr);

    // コマンドリストが開いていない場合のみ一時的に開く
    bool needSync = !dxContext->IsCommandListOpen();
    if (needSync) {
        dxContext->ResetCommandList();
    }
    auto commandList = dxContext->GetCommandList();

    // Buffer 0 & 1: COMMON -> COPY_DEST
    D3D12_RESOURCE_BARRIER initBarriers[2]{};
    for (int i = 0; i < 2; ++i) {
        initBarriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        initBarriers[i].Transition.pResource = droneBuffers_[i].Get();
        initBarriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        initBarriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        initBarriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    }
    commandList->ResourceBarrier(2, initBarriers);

    commandList->CopyBufferRegion(droneBuffers_[0].Get(), 0, intermediateUploadBuffer_.Get(), 0, bufferSize);
    commandList->CopyBufferRegion(droneBuffers_[1].Get(), 0, intermediateUploadBuffer_.Get(), 0, bufferSize);

    // Buffer 0 & 1: COPY_DEST -> NON_PIXEL_SHADER_RESOURCE
    for (int i = 0; i < 2; ++i) {
        initBarriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        initBarriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }
    commandList->ResourceBarrier(2, initBarriers);

    // 一時的に開いた場合のみ即座に実行・同期して閉じる
    if (needSync) {
        dxContext->ExecuteInitialCommandAndSync();
    }

    // 4. デスクリプタの割り当て (SRV x 2, UAV x 2)
    for (int i = 0; i < 2; ++i) {
        srvIndices_[i] = srvManager->Allocate();
        srvManager->CreateSRVForStructuredBuffer(
            srvIndices_[i], droneBuffers_[i], droneCount_, sizeof(Swarm::DroneData));

        uavIndices_[i] = srvManager->Allocate();
        srvManager->CreateUAVForStructuredBuffer(
            uavIndices_[i], droneBuffers_[i], droneCount_, sizeof(Swarm::DroneData));
    }

    // 5. 定数バッファの生成
    constantBuffer_ = dxContext->CreateBufferResource(sizeof(Swarm::SwarmGlobalConstants));
    constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedConstants_));

    cameraConstantBuffer_ = dxContext->CreateBufferResource(sizeof(CameraTransformBuffer));
    cameraConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedCameraConstants_));

    readIndex_ = 0;
    writeIndex_ = 1;
}

void SwarmManager::InitializePipeline() {
    auto dxContext = DX12Context::GetInstance();
    auto pipelineManager = PipelineManager::GetInstance();

    // ==========================================
    // 1. Compute Pipeline (SwarmSimulate.CS)
    // ==========================================
    {
        D3D12_DESCRIPTOR_RANGE srvRange{};
        srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors = 1;
        srvRange.BaseShaderRegister = 0;
        srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE uavRange{};
        uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRange.NumDescriptors = 1;
        uavRange.BaseShaderRegister = 0;
        uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER rootParams[3]{};
        rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[0].Descriptor.ShaderRegister = 0;

        rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[1].DescriptorTable.pDescriptorRanges = &srvRange;

        rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[2].DescriptorTable.pDescriptorRanges = &uavRange;

        D3D12_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.NumParameters = _countof(rootParams);
        rsDesc.pParameters = rootParams;
        rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        computeRootSignature_ = pipelineManager->CreateComputeRootSignature(rsDesc);

        auto csBlob = dxContext->CompileShader(L"Resources/Shaders/Swarm/SwarmSimulate.CS.hlsl", L"cs_6_0");
        computePSO_ = pipelineManager->CreateComputePSO(computeRootSignature_.Get(), csBlob.Get());
    }

    // ==========================================
    // 2. Draw Pipeline (SwarmDraw.VS / PS)
    // ==========================================
    {
        D3D12_DESCRIPTOR_RANGE droneRange{};
        droneRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        droneRange.NumDescriptors = 1;
        droneRange.BaseShaderRegister = 0;
        droneRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE texRange{};
        texRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        texRange.NumDescriptors = 1;
        texRange.BaseShaderRegister = 1;
        texRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE maskRange{};
        maskRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        maskRange.NumDescriptors = 1;
        maskRange.BaseShaderRegister = 2;
        maskRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER rootParams[4]{};
        rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[0].DescriptorTable.pDescriptorRanges = &droneRange;

        rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[1].Descriptor.ShaderRegister = 1;

        rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[2].DescriptorTable.pDescriptorRanges = &texRange;

        rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[3].DescriptorTable.pDescriptorRanges = &maskRange;

        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.ShaderRegister = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.NumParameters = _countof(rootParams);
        rsDesc.pParameters = rootParams;
        rsDesc.NumStaticSamplers = 1;
        rsDesc.pStaticSamplers = &sampler;
        rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signatureBlob = nullptr;
        ComPtr<ID3DBlob> errorBlob = nullptr;
        HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
        assert(SUCCEEDED(hr));
        hr = dxContext->GetDevice()->CreateRootSignature(
            0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&drawRootSignature_));
        assert(SUCCEEDED(hr));

        auto vsBlob = dxContext->CompileShader(L"Resources/Shaders/Swarm/SwarmDraw.VS.hlsl", L"vs_6_0");
        auto psBlob = dxContext->CompileShader(L"Resources/Shaders/Swarm/SwarmDraw.PS.hlsl", L"ps_6_0");

        D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = drawRootSignature_.Get();
        psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        psoDesc.SampleDesc.Count = 1;

        hr = dxContext->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&drawPSO_));
        assert(SUCCEEDED(hr));
    }
}

void SwarmManager::InitializeModel(const std::string& directory, const std::string& filename) {
    droneModel_ = std::make_unique<Model>();
    droneModel_->Initialize(directory, filename);

    std::string texPath = droneModel_->GetTextureFilePath();
    if (!texPath.empty()) {
        textureSrvIndex_ = TextureManager::GetInstance()->GetSrvIndex(texPath);
    } else {
        textureSrvIndex_ = TextureManager::GetInstance()->GetSrvIndex("white1x1.png");
    }

    TextureManager::GetInstance()->LoadTexture("masks/noise0.png");
    dissolveMaskSrvIndex_ = TextureManager::GetInstance()->GetSrvIndex("masks/noise0.png");
}

void SwarmManager::Update(float deltaTime,
                          const Vector3 & playerPos,
                          Camera* camera,
                          const AttackCommand* attackCmd,
                          bool isPlayerCovering) {
    frameEvents_ = {};
    if (!isActive_) {
        return;
    }

    settings_.playerPosition = playerPos;
    settings_.deltaTime = deltaTime;
    settings_.droneCount = droneCount_;

    if (camera) {
        Vector3 camT = camera->GetTranslate();
        settings_.cameraPosition = { camT.x, camT.y, camT.z };

        Matrix4x4 vp = camera->GetViewProjectionMatrix();
        std::memcpy(&mappedCameraConstants_->viewProjection, &vp, sizeof(Matrix4x4));
        mappedCameraConstants_->cameraPosition = settings_.cameraPosition;
        mappedCameraConstants_->droneScale = droneScale_;
        mappedCameraConstants_->crashDissolveThreshold = crashDissolveThreshold_;
        mappedCameraConstants_->dissolveEdgeRange = 0.08f;
        mappedCameraConstants_->dissolveEdgeColor = { 0.0f, 1.0f, 0.8f };
    }

    if (isBossMode_) {
        UpdateBossSimulation(deltaTime, playerPos, attackCmd, isPlayerCovering);
        needGpuSync_ = true;
    } else {
        UpdateIntroSimulation(deltaTime, playerPos);
        needGpuSync_ = true;
    }

    *mappedConstants_ = settings_;
}

void SwarmManager::UpdateIntroSimulation(float deltaTime, const Vector3& targetCenter) {
    for (uint32_t i = 0; i < droneCount_; ++i) {
        auto& self = dronesShadow_[i];
        if (self.hp <= 0.0f || self.state == 2) continue;

        Vector3 toCenter = MathUtils::Subtract(targetCenter, self.position);
        float dist = MathUtils::Length(toCenter);
        Vector3 dir = (dist > 0.001f) ? MathUtils::Multiply(1.0f / dist, toCenter) : Vector3{ 0.0f, 0.0f, 1.0f };

        // 旋回＋中心引き寄せ
        Vector3 tangent = { -dir.z, 0.0f, dir.x };
        Vector3 desiredVel = MathUtils::Add(
            MathUtils::Multiply(settings_.maxSpeed * 0.8f, tangent),
            MathUtils::Multiply((dist - 12.0f) * 0.8f, dir)
        );
        self.velocity = MathUtils::Add(self.velocity, MathUtils::Multiply(deltaTime * 2.5f, MathUtils::Subtract(desiredVel, self.velocity)));
        float spd = MathUtils::Length(self.velocity);
        if (spd > settings_.maxSpeed) {
            self.velocity = MathUtils::Multiply(settings_.maxSpeed / spd, self.velocity);
        }
        self.position = MathUtils::Add(self.position, MathUtils::Multiply(deltaTime, self.velocity));
    }
}

void SwarmManager::StartBossBattle(uint32_t count, const Vector3& playerBasePos, float baseYaw, bool keepExistingPositions) {
    uint32_t actualCount = (count <= droneCount_) ? count : droneCount_;

    isBossMode_ = true;
    isBossDefeated_ = false;
    ChangeBossPhaseState(std::make_unique<BossPhase1State>());
    crashDissolveThreshold_ = 0.0f;
    bossMaxHp_ = actualCount;
    bossPlayerBasePos_ = playerBasePos;
    bossGroundY_ = playerBasePos.y - 1.4f;
    bossBaseYaw_ = baseYaw;
    bossMoveTimer_ = 0.0f;
    noShootTimer_ = 0.0f;
    periodicAttackTimer_ = 0.0f;
    evadeCooldownTimer_ = 0.0f;
    currentSection_ = 0;
    chargeGroups_.clear();
    frameEvents_ = {};

    droneScale_ = 0.32f;
    settings_.maxSpeed = 14.0f;
    settings_.ChargeSpeed = 16.0f;
    settings_.neighborRadius = 5.5f;
    settings_.separationDist = 1.5f;
    settings_.droneCount = actualCount;

    if (!keepExistingPositions || dronesShadow_.size() != droneCount_) {
        Vector3 forward = { std::sin(bossBaseYaw_), 0.0f, std::cos(bossBaseYaw_) };
        Vector3 right = { std::cos(bossBaseYaw_), 0.0f, -std::sin(bossBaseYaw_) };
        Vector3 up = { 0.0f, 1.0f, 0.0f };

        // 正面少し上（前方22m, 高さ+4.5m）を中心に配置
        Vector3 anchorCenter = MathUtils::Add(
            bossPlayerBasePos_,
            MathUtils::Add(MathUtils::Multiply(22.0f, forward), MathUtils::Multiply(4.5f, up))
        );

        dronesShadow_.resize(droneCount_);
        crashSleeping_.assign(droneCount_, false);
        std::mt19937 mt(1337);
        std::uniform_real_distribution<float> xDist(-4.5f, 4.5f);
        std::uniform_real_distribution<float> yDist(-1.6f, 1.8f);
        std::uniform_real_distribution<float> zDist(-2.0f, 2.0f);
        std::uniform_real_distribution<float> vDist(-3.0f, 3.0f);

        for (uint32_t i = 0; i < droneCount_; ++i) {
            if (i < actualCount) {
                float ox = xDist(mt);
                float oy = yDist(mt);
                float oz = zDist(mt);
                Vector3 offset = MathUtils::Add(
                    MathUtils::Multiply(ox, right),
                    MathUtils::Add(MathUtils::Multiply(oy, up), MathUtils::Multiply(oz, forward))
                );
                dronesShadow_[i].position = MathUtils::Add(anchorCenter, offset);
                dronesShadow_[i].velocity = MathUtils::Add(
                    MathUtils::Multiply(6.0f, right),
                    { vDist(mt), vDist(mt) * 0.4f, vDist(mt) }
                );
                dronesShadow_[i].hp = 10.0f;
                dronesShadow_[i].state = 0; // Cruise
                dronesShadow_[i].target = anchorCenter;
                dronesShadow_[i].stateTimer = 0.0f;
            } else {
                dronesShadow_[i].position = { 0.0f, -999.0f, 0.0f };
                dronesShadow_[i].velocity = { 0.0f, 0.0f, 0.0f };
                dronesShadow_[i].hp = 0.0f;
                dronesShadow_[i].state = 2; // Dead
                dronesShadow_[i].target = { 0.0f, 0.0f, 0.0f };
                dronesShadow_[i].stateTimer = 0.0f;
            }
        }

        pendingSpawnData_ = dronesShadow_;
        pendingSpawn_ = true;
    } else {
        crashSleeping_.assign(droneCount_, false);
        for (uint32_t i = 0; i < droneCount_; ++i) {
            if (i < actualCount) {
                dronesShadow_[i].hp = 10.0f;
                dronesShadow_[i].state = 0; // Cruise
                dronesShadow_[i].stateTimer = 0.0f;
            } else {
                dronesShadow_[i].position = { 0.0f, -999.0f, 0.0f };
                dronesShadow_[i].velocity = { 0.0f, 0.0f, 0.0f };
                dronesShadow_[i].hp = 0.0f;
                dronesShadow_[i].state = 2; // Dead
            }
        }
    }

    needGpuSync_ = true;
    isActive_ = true;
}

void SwarmManager::StartBossEntrance(uint32_t count, const Vector3& playerBasePos, float baseYaw, const Vector3& spawnPos) {
    uint32_t actualCount = (count <= droneCount_) ? count : droneCount_;

    isBossMode_ = false;
    isBossDefeated_ = false;
    ChangeBossPhaseState(std::make_unique<BossPhase1State>());
    crashDissolveThreshold_ = 0.0f;
    bossMaxHp_ = actualCount;
    bossPlayerBasePos_ = playerBasePos;
    bossGroundY_ = playerBasePos.y - 1.4f;
    bossBaseYaw_ = baseYaw;
    bossMoveTimer_ = 0.0f;
    chargeGroups_.clear();
    frameEvents_ = {};

    droneScale_ = 0.32f;
    settings_.maxSpeed = 28.0f;
    settings_.droneCount = actualCount;

    dronesShadow_.resize(droneCount_);
    crashSleeping_.assign(droneCount_, false);
    std::mt19937 mt(2026);
    std::uniform_real_distribution<float> spreadDist(-3.0f, 3.0f);

    for (uint32_t i = 0; i < droneCount_; ++i) {
        if (i < actualCount) {
            dronesShadow_[i].position = {
                spawnPos.x + spreadDist(mt),
                spawnPos.y + spreadDist(mt) * 0.6f,
                spawnPos.z + spreadDist(mt)
            };
            dronesShadow_[i].velocity = { 0.0f, 0.0f, 0.0f };
            dronesShadow_[i].hp = 10.0f;
            dronesShadow_[i].state = 0;
            dronesShadow_[i].target = spawnPos;
            dronesShadow_[i].stateTimer = static_cast<float>(i) * 0.37f;
        } else {
            dronesShadow_[i].position = { 0.0f, -999.0f, 0.0f };
            dronesShadow_[i].velocity = { 0.0f, 0.0f, 0.0f };
            dronesShadow_[i].hp = 0.0f;
            dronesShadow_[i].state = 2;
            dronesShadow_[i].target = { 0.0f, 0.0f, 0.0f };
            dronesShadow_[i].stateTimer = 0.0f;
        }
    }

    pendingSpawnData_ = dronesShadow_;
    pendingSpawn_ = true;
    needGpuSync_ = true;
    isActive_ = true;
}

void SwarmManager::UpdateBossEntrance(float deltaTime, const Vector3& anchorPos, Camera* camera) {
    frameEvents_ = {};
    if (!isActive_) return;

    bossMoveTimer_ += deltaTime;

    if (camera) {
        Vector3 camT = camera->GetTranslate();
        settings_.cameraPosition = { camT.x, camT.y, camT.z };
        Matrix4x4 vp = camera->GetViewProjectionMatrix();
        std::memcpy(&mappedCameraConstants_->viewProjection, &vp, sizeof(Matrix4x4));
        mappedCameraConstants_->cameraPosition = settings_.cameraPosition;
        mappedCameraConstants_->droneScale = droneScale_;
        mappedCameraConstants_->crashDissolveThreshold = crashDissolveThreshold_;
        mappedCameraConstants_->dissolveEdgeRange = 0.08f;
        mappedCameraConstants_->dissolveEdgeColor = { 0.0f, 1.0f, 0.8f };
    }

    const float kFollowSpeed = 30.0f;
    for (uint32_t i = 0; i < droneCount_; ++i) {
        auto& self = dronesShadow_[i];
        if (self.hp <= 0.0f || self.state == 2) continue;

        // 群れのばらつきを保ちつつアンカーへ追従
        float phase = bossMoveTimer_ * 3.5f + self.stateTimer;
        Vector3 swarmOffset = {
            std::sin(phase) * 2.2f,
            std::cos(phase * 1.3f) * 1.1f,
            std::sin(phase * 0.7f + 1.0f) * 1.8f
        };
        Vector3 targetPos = MathUtils::Add(anchorPos, swarmOffset);
        Vector3 toTarget = MathUtils::Subtract(targetPos, self.position);
        float dist = MathUtils::Length(toTarget);
        Vector3 desiredVel = (dist > 0.001f)
            ? MathUtils::Multiply((std::min)(kFollowSpeed, dist * 6.5f), MathUtils::Multiply(1.0f / dist, toTarget))
            : Vector3{ 0.0f, 0.0f, 0.0f };

        self.velocity = MathUtils::Add(self.velocity, MathUtils::Multiply((std::min)(1.0f, deltaTime * 8.0f), MathUtils::Subtract(desiredVel, self.velocity)));
        self.position = MathUtils::Add(self.position, MathUtils::Multiply(deltaTime, self.velocity));
    }

    needGpuSync_ = true;
    *mappedConstants_ = settings_;
}

void SwarmManager::CreateChargeGroup(const std::vector<uint32_t>& indices, bool isFinalGroup, const Vector3& playerPos) {
    if (indices.empty()) return;

    ChargeGroup group;
    group.droneIndices = indices;
    group.isFinalGroup = isFinalGroup;
    group.maxHp = static_cast<int>(indices.size());
    group.hp = group.maxHp;
    group.damageBarRatio = 1.0f;
    group.isDead = false;
    group.isHpGaugeVisible = true;

    Vector3 forward = { std::sin(bossBaseYaw_), 0.0f, std::cos(bossBaseYaw_) };
    Vector3 right = { std::cos(bossBaseYaw_), 0.0f, -std::sin(bossBaseYaw_) };
    Vector3 up = { 0.0f, 1.0f, 0.0f };

    // グループ中心の算出
    Vector3 sumPos = { 0.0f, 0.0f, 0.0f };
    for (uint32_t idx : indices) {
        sumPos = MathUtils::Add(sumPos, dronesShadow_[idx].position);
        dronesShadow_[idx].state = 1; // Charge (赤発光)
        dronesShadow_[idx].stateTimer = 0.0f;
    }
    group.centerPos = MathUtils::Multiply(1.0f / static_cast<float>(indices.size()), sumPos);

    // 最終突撃グループの場合は確実に正面少し上から突撃開始できるよう距離を確保
    if (isFinalGroup) {
        Vector3 toGroup = MathUtils::Subtract(group.centerPos, playerPos);
        if (MathUtils::Length(toGroup) < 18.0f) {
            group.centerPos = MathUtils::Add(
                bossPlayerBasePos_,
                MathUtils::Add(MathUtils::Multiply(21.0f, forward), MathUtils::Multiply(4.0f, up))
            );
        }
        group.speed = 4.2f;     // 15発（リロード1回含む）撃ち込める速度
        group.hitRadius = 2.8f; // まとめた塊への当たり判定半径
    } else {
        // 第二形態の突撃グループ（3機ならやや速め、10機なら中速）
        group.speed = (indices.size() <= 4) ? 6.8f : 4.8f;
        group.hitRadius = (indices.size() <= 4) ? 2.0f : 2.5f;
    }

    // 小さめにまとめた編隊オフセットを生成
    float clusterRadius = isFinalGroup ? 1.5f : ((indices.size() <= 4) ? 0.9f : 1.3f);
    group.localOffsets.resize(indices.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        if (indices.size() == 1) {
            group.localOffsets[i] = { 0.0f, 0.0f, 0.0f };
        } else {
            // フィボナッチ球面配置で小さな塊に密集させる
            float fi = static_cast<float>(i);
            float fn = static_cast<float>(indices.size());
            float y = 1.0f - (fi / (fn - 1.0f)) * 2.0f;
            float r = std::sqrt((std::max)(0.0f, 1.0f - y * y));
            float theta = fi * 2.3999632f; // 黄金角
            float rad = clusterRadius * (0.55f + 0.45f * ((i % 2 == 0) ? 1.0f : 0.7f));
            Vector3 local = {
                std::cos(theta) * r * rad,
                y * rad * 0.8f,
                std::sin(theta) * r * rad
            };
            group.localOffsets[i] = MathUtils::Add(
                MathUtils::Multiply(local.x, right),
                MathUtils::Add(MathUtils::Multiply(local.y, up), MathUtils::Multiply(local.z, forward))
            );
        }
    }

    // 頭上HPゲージスプライトの初期化
    group.hpBgSprite = std::make_unique<Sprite>();
    group.hpBgSprite->Initialize("white.png");
    group.hpBgSprite->SetAnchorPoint({ 0.0f, 0.5f });
    group.hpBgSprite->SetColor({ 0.08f, 0.08f, 0.08f, 0.85f });

    group.hpDamageSprite = std::make_unique<Sprite>();
    group.hpDamageSprite->Initialize("white.png");
    group.hpDamageSprite->SetAnchorPoint({ 0.0f, 0.5f });
    group.hpDamageSprite->SetColor({ 1.0f, 0.2f, 0.1f, 0.9f });

    group.hpFillSprite = std::make_unique<Sprite>();
    group.hpFillSprite->Initialize("white.png");
    group.hpFillSprite->SetAnchorPoint({ 0.0f, 0.5f });
    group.hpFillSprite->SetColor({ 1.0f, 0.85f, 0.1f, 1.0f });

    group.hpFrameSprite = std::make_unique<Sprite>();
    group.hpFrameSprite->Initialize("ui/enemyHpFrame.png");
    group.hpFrameSprite->SetAnchorPoint({ 0.5f, 0.5f });
    group.hpFrameSprite->SetScale({ 90.0f, 16.0f });

    frameEvents_.chargeStartPositions.push_back(group.centerPos);
    chargeGroups_.push_back(std::move(group));
}

void SwarmManager::TriggerCharge(uint32_t count, const Vector3& playerPos) {
    if (!isActive_ || !isBossMode_ || !bossPhaseState_ || !bossPhaseState_->CanTriggerNormalCharge()) return;

    uint32_t aliveCount = GetAliveDroneCount();
    if (aliveCount <= 15) return; // 15機以下は最終突撃フェーズで一括処理

    // 現在巡航中（state == 0）のドローンを収集
    struct Candidate {
        uint32_t index;
        float distSq;
    };
    std::vector<Candidate> candidates;
    for (uint32_t i = 0; i < droneCount_; ++i) {
        const auto& d = dronesShadow_[i];
        if (d.hp > 0.0f && d.state == 0) {
            Vector3 diff = MathUtils::Subtract(d.position, playerPos);
            float dSq = MathUtils::Dot(diff, diff);
            candidates.push_back({ i, dSq });
        }
    }

    // 最後の15機は最終突撃用に温存するため、巡航機が15機以下にならない範囲で選出
    if (candidates.size() <= 15) return;
    uint32_t maxSelectable = static_cast<uint32_t>(candidates.size() - 15);
    uint32_t selectCount = (std::min)(count, maxSelectable);
    if (selectCount == 0) return;

    // プレイヤーに最も近い順にソート
    std::partial_sort(candidates.begin(), candidates.begin() + selectCount, candidates.end(),
                      [](const Candidate& a, const Candidate& b) { return a.distSq < b.distSq; });

    std::vector<uint32_t> selectedIndices;
    selectedIndices.reserve(selectCount);
    for (uint32_t i = 0; i < selectCount; ++i) {
        selectedIndices.push_back(candidates[i].index);
    }

    bossPhaseState_->ExecuteCharge(*this, selectedIndices, playerPos);
}

void SwarmManager::UpdateBossSimulation(float deltaTime, const Vector3& playerPos, const AttackCommand* attackCmd, bool isPlayerCovering) {
    Vector3 forward = { std::sin(bossBaseYaw_), 0.0f, std::cos(bossBaseYaw_) };
    Vector3 right = { std::cos(bossBaseYaw_), 0.0f, -std::sin(bossBaseYaw_) };
    Vector3 up = { 0.0f, 1.0f, 0.0f };

    if (evadeCooldownTimer_ > 0.0f) {
        evadeCooldownTimer_ -= deltaTime;
        if (evadeCooldownTimer_ < 0.0f) evadeCooldownTimer_ = 0.0f;
    }

    // =========================================================
    // 1. プレイヤーの射撃（レイキャスト）判定 ＆ 被弾回避（クールタイム1秒）
    // =========================================================
    if (attackCmd && attackCmd->damage > 0.0f) {
        // 射撃が行われたので2秒未射撃タイマーをリセット
        noShootTimer_ = 0.0f;

        Vector3 rayOrigin = attackCmd->rayOrigin;
        Vector3 rayDir = attackCmd->rayDirection;

        // 1-A. 突撃グループ（第二形態・最終形態）との当たり判定
        float closestGroupT = (std::numeric_limits<float>::max)();
        int hitGroupIdx = -1;

        for (size_t gIdx = 0; gIdx < chargeGroups_.size(); ++gIdx) {
            auto& group = chargeGroups_[gIdx];
            if (group.isDead) continue;

            // グループ中心球、または構成ドローンのいずれかにレイが当たったか
            Vector3 toCenter = MathUtils::Subtract(group.centerPos, rayOrigin);
            float t = MathUtils::Dot(toCenter, rayDir);
            bool hit = false;
            if (t > 0.0f) {
                Vector3 closestPt = MathUtils::Add(rayOrigin, MathUtils::Multiply(t, rayDir));
                float dist = MathUtils::Length(MathUtils::Subtract(group.centerPos, closestPt));
                if (dist <= group.hitRadius) {
                    hit = true;
                }
            }
            if (!hit) {
                for (uint32_t dIdx : group.droneIndices) {
                    const auto& d = dronesShadow_[dIdx];
                    if (d.hp <= 0.0f || d.state == 2) continue;
                    Vector3 toDrone = MathUtils::Subtract(d.position, rayOrigin);
                    float dt = MathUtils::Dot(toDrone, rayDir);
                    if (dt > 0.0f) {
                        Vector3 cPt = MathUtils::Add(rayOrigin, MathUtils::Multiply(dt, rayDir));
                        if (MathUtils::Length(MathUtils::Subtract(d.position, cPt)) <= 1.6f) {
                            hit = true;
                            t = dt;
                            break;
                        }
                    }
                }
            }

            if (hit && t < closestGroupT) {
                closestGroupT = t;
                hitGroupIdx = static_cast<int>(gIdx);
            }
        }

        // 1-B. 個別ドローン（グループ外）との当たり判定
        // グループに所属しているドローンを除外するためのフラグ
        std::vector<bool> inActiveGroup(droneCount_, false);
        for (const auto& group : chargeGroups_) {
            if (group.isDead) continue;
            for (uint32_t dIdx : group.droneIndices) {
                if (dIdx < droneCount_) inActiveGroup[dIdx] = true;
            }
        }

        // 巡航ドローンの総数をカウント（アクティブな突撃グループが存在する間、最後の15機の巡航機は最終突撃用に保護）
        uint32_t cruisingCount = 0;
        for (uint32_t i = 0; i < droneCount_; ++i) {
            if (!inActiveGroup[i] && dronesShadow_[i].hp > 0.0f && dronesShadow_[i].state == 0) {
                cruisingCount++;
            }
        }
        bool hasActiveNonFinalGroup = false;
        for (const auto& group : chargeGroups_) {
            if (!group.isDead && !group.isFinalGroup) {
                hasActiveNonFinalGroup = true;
                break;
            }
        }

        float closestDroneT = (std::numeric_limits<float>::max)();
        int hitDroneIdx = -1;

        for (uint32_t i = 0; i < droneCount_; ++i) {
            if (inActiveGroup[i]) continue;
            const auto& d = dronesShadow_[i];
            if (d.hp <= 0.0f || d.state == 2) continue;

            // 第二形態突撃グループが残っている状態で巡航機が残り15機以下の場合は、先に突撃グループを撃破させる
            if (d.state == 0 && hasActiveNonFinalGroup && cruisingCount <= 15) {
                continue;
            }

            Vector3 toDrone = MathUtils::Subtract(d.position, rayOrigin);
            float t = MathUtils::Dot(toDrone, rayDir);
            if (t > 0.0f && t < closestDroneT) {
                Vector3 closestPt = MathUtils::Add(rayOrigin, MathUtils::Multiply(t, rayDir));
                float dist = MathUtils::Length(MathUtils::Subtract(d.position, closestPt));
                float hitRad = (d.state == 1) ? 1.8f : 1.5f;
                if (dist <= hitRad) {
                    closestDroneT = t;
                    hitDroneIdx = static_cast<int>(i);
                }
            }
        }

        // 手前にあるターゲットへダメージ適用
        if (hitGroupIdx >= 0 && closestGroupT <= closestDroneT) {
            auto& group = chargeGroups_[hitGroupIdx];
            group.hp -= 1;
            frameEvents_.hitPositions.push_back(group.centerPos);

            if (group.hp <= 0) {
                group.hp = 0;
                group.isDead = true;
                group.isHpGaugeVisible = false;
                for (uint32_t dIdx : group.droneIndices) {
                    auto& cd = dronesShadow_[dIdx];
                    cd.hp = 0.0f;
                    cd.state = 3; // Crashing
                    cd.stateTimer = 0.0f;
                    if (dIdx < crashSleeping_.size()) crashSleeping_[dIdx] = false;
                    float spreadX = (static_cast<float>(dIdx % 5) - 2.0f) * 2.0f;
                    float spreadZ = (static_cast<float>((dIdx / 5) % 5) - 2.0f) * 2.0f;
                    cd.velocity = {
                        spreadX,
                        2.5f + static_cast<float>(dIdx % 4) * 1.1f,
                        spreadZ
                    };
                }
                frameEvents_.destroyedPositions.push_back(group.centerPos);
                frameEvents_.dronesKilledByPlayer += static_cast<int>(group.droneIndices.size());

                // 最終突撃グループの体力ゲージを削り切ったらボスクリア！
                if (group.isFinalGroup) {
                    isBossDefeated_ = true;
                    frameEvents_.bossDefeated = true;
                }
            }
        } else if (hitDroneIdx >= 0) {
            auto& d = dronesShadow_[hitDroneIdx];
            Vector3 hitPos = d.position;
            d.hp = 0.0f;
            d.state = 3; // Crashing
            d.stateTimer = 0.0f;
            if (static_cast<size_t>(hitDroneIdx) < crashSleeping_.size()) crashSleeping_[hitDroneIdx] = false;
            float spreadX = (static_cast<float>(hitDroneIdx % 3) - 1.0f) * 2.5f;
            float spreadZ = (static_cast<float>((hitDroneIdx / 3) % 3) - 1.0f) * 2.5f;
            d.velocity = {
                d.velocity.x * 0.4f + spreadX,
                2.0f,
                d.velocity.z * 0.4f + spreadZ
            };
            frameEvents_.destroyedPositions.push_back(hitPos);
            frameEvents_.dronesKilledByPlayer += 1;
        }

        // 1-C. 射撃に対する回避行動（1秒クールタイムで撃たれた位置を避ける動き）
        if (evadeCooldownTimer_ <= 0.0f) {
            evadeCooldownTimer_ = 1.0f; // 1秒間のクールタイム
            const float kEvadeRadius = 4.5f;

            for (uint32_t i = 0; i < droneCount_; ++i) {
                if (inActiveGroup[i]) continue;
                auto& d = dronesShadow_[i];
                if (d.hp <= 0.0f || d.state != 0) continue; // 巡航中のドローンのみ回避

                Vector3 toDrone = MathUtils::Subtract(d.position, rayOrigin);
                float t = MathUtils::Dot(toDrone, rayDir);
                if (t > 0.0f) {
                    Vector3 closestPt = MathUtils::Add(rayOrigin, MathUtils::Multiply(t, rayDir));
                    Vector3 diff = MathUtils::Subtract(d.position, closestPt);
                    float dist = MathUtils::Length(diff);

                    if (dist < kEvadeRadius) {
                        Vector3 evadeDir = (dist > 0.001f) ? MathUtils::Normalize(diff) : right;
                        // 前後方向への飛び出しを抑え、主に左右・上下へ散開させる
                        float fwdComp = MathUtils::Dot(evadeDir, forward);
                        evadeDir = MathUtils::Subtract(evadeDir, MathUtils::Multiply(fwdComp * 0.85f, forward));
                        if (MathUtils::Length(evadeDir) > 0.001f) {
                            evadeDir = MathUtils::Normalize(evadeDir);
                        } else {
                            evadeDir = (i % 2 == 0) ? right : MathUtils::Multiply(-1.0f, right);
                        }

                        float ratio = 1.0f - (dist / kEvadeRadius);
                        float impulse = 7.5f + 5.5f * ratio;
                        d.velocity = MathUtils::Add(d.velocity, MathUtils::Multiply(impulse, evadeDir));
                        d.stateTimer = -0.25f; // 短時間だけ軽く避け、画面外へ散らばらないように抑える
                    }
                }
            }
        }
    }

    // 墜落中・残骸ドローンの物理・地面・ドローン間コリジョン更新
    UpdateCrashingDrones(deltaTime);

    // ボス撃破済みならここで終了
    if (isBossDefeated_) {
        return;
    }

    // =========================================================
    // 2. ボス形態フェーズの遷移チェック ＆ 攻撃タイマー更新 (State Pattern)
    // =========================================================
    if (!bossPhaseState_) {
        ChangeBossPhaseState(std::make_unique<BossPhase1State>(), playerPos);
    }
    bossPhaseState_->Update(*this, deltaTime, playerPos);

    // =========================================================
    // 3. 突撃グループ（第二形態・最終形態）の移動と到達判定
    // =========================================================
    std::vector<bool> inActiveGroup(droneCount_, false);
    Vector3 playerTarget = MathUtils::Add(bossPlayerBasePos_, MathUtils::Multiply(0.3f, up));

    for (auto& group : chargeGroups_) {
        if (group.isDead) continue;

        Vector3 toPlayer = MathUtils::Subtract(playerTarget, group.centerPos);
        float distToPlayer = MathUtils::Length(toPlayer);
        Vector3 dirToPlayer = (distToPlayer > 0.001f)
            ? MathUtils::Multiply(1.0f / distToPlayer, toPlayer)
            : MathUtils::Multiply(-1.0f, forward);

        group.velocity = MathUtils::Multiply(group.speed, dirToPlayer);
        group.centerPos = MathUtils::Add(group.centerPos, MathUtils::Multiply(deltaTime, group.velocity));

        // プレイヤーに到達したらダメージ判定
        if (distToPlayer < 2.3f) {
            frameEvents_.selfDestructPositions.push_back(group.centerPos);

            if (isPlayerCovering) {
                // カバー時：爆発・自傷あり、プレイヤー爆風ダメージ3、ボスの体力と数を1減らし、残りを帰還状態にする
                frameEvents_.playerGroupBlastHits += 1;

                bool sacrificedOne = false;
                uint32_t remainingAliveInGroup = 0;
                for (size_t i = 0; i < group.droneIndices.size(); ++i) {
                    uint32_t dIdx = group.droneIndices[i];
                    auto& d = dronesShadow_[dIdx];
                    if (d.hp > 0.0f && d.state != 2) {
                        if (!sacrificedOne) {
                            d.hp = 0.0f;
                            d.state = 2; // Dead (1機分だけ撃破・ボス体力1減)
                            sacrificedOne = true;
                        } else {
                            d.state = 4; // Returning (帰還状態)
                            d.stateTimer = 0.0f;
                            d.velocity = MathUtils::Add(MathUtils::Multiply(8.0f, forward), { 0.0f, 9.0f, 0.0f });
                            remainingAliveInGroup++;
                        }
                    }
                }

                // グループ化を解除して残りは本隊へ帰還（最終グループで全機消滅した場合は撃破）
                group.isDead = true;
                group.isHpGaugeVisible = false;
                if (group.isFinalGroup && remainingAliveInGroup == 0) {
                    isBossDefeated_ = true;
                    frameEvents_.bossDefeated = true;
                }
                continue;
            } else {
                // 通常時（非カバー）：プレイヤーに直撃ダメージ10
                frameEvents_.playerDirectHits += 1;

                if (group.isFinalGroup) {
                    // 最終15機グループは通常被弾時も正面奥へワープさせず、帰還状態にして本隊アンカーへ戻す
                    for (uint32_t dIdx : group.droneIndices) {
                        auto& d = dronesShadow_[dIdx];
                        if (d.hp > 0.0f && d.state != 2) {
                            d.state = 4; // Returning
                            d.stateTimer = 0.0f;
                            d.velocity = MathUtils::Add(MathUtils::Multiply(8.0f, forward), { 0.0f, 9.0f, 0.0f });
                        }
                    }
                    group.isDead = true;
                    group.isHpGaugeVisible = false;
                    continue;
                } else {
                    // 第二形態の通常突撃グループは自爆消滅し、機体分ボス全体HPが減る
                    group.isDead = true;
                    group.isHpGaugeVisible = false;
                    for (uint32_t dIdx : group.droneIndices) {
                        dronesShadow_[dIdx].hp = 0.0f;
                        dronesShadow_[dIdx].state = 2; // Dead
                    }
                    continue;
                }
            }
        }

        // グループ内の各ドローンを小さな塊に維持して移動
        float wobblePhase = bossMoveTimer_ * 4.0f;
        for (size_t i = 0; i < group.droneIndices.size(); ++i) {
            uint32_t dIdx = group.droneIndices[i];
            inActiveGroup[dIdx] = true;
            auto& d = dronesShadow_[dIdx];
            if (d.hp <= 0.0f || d.state == 2) continue;

            Vector3 wobble = {
                std::sin(wobblePhase + static_cast<float>(i) * 1.7f) * 0.15f,
                std::cos(wobblePhase * 1.3f + static_cast<float>(i)) * 0.15f,
                0.0f
            };
            Vector3 targetPos = MathUtils::Add(group.centerPos, MathUtils::Add(group.localOffsets[i], wobble));
            d.position = MathUtils::Add(d.position, MathUtils::Multiply((std::min)(1.0f, deltaTime * 12.0f), MathUtils::Subtract(targetPos, d.position)));
            d.velocity = group.velocity;
            d.state = 1; // Charge
        }
    }

    // 死んだグループを掃除
    chargeGroups_.erase(
        std::remove_if(chargeGroups_.begin(), chargeGroups_.end(),
                       [](const ChargeGroup& g) { return g.isDead; }),
        chargeGroups_.end());

    // グループ自爆によって残り15機以下になった場合も即座に最終突撃グループへ移行
    if (bossPhaseState_) {
        bossPhaseState_->CheckTransition(*this, playerPos);
        if (bossPhaseState_->GetPhaseNumber() == 3) {
            for (const auto& group : chargeGroups_) {
                if (group.isDead) continue;
                for (uint32_t dIdx : group.droneIndices) {
                    if (dIdx < droneCount_) {
                        inActiveGroup[dIdx] = true;
                    }
                }
            }
        }
    }

    // =========================================================
    // 4. 左右往復アンカー計算と個別ドローン（巡航 / 第一形態突撃）の更新
    // =========================================================
    // 突撃中（個別突撃機または突撃グループが存在する間）か判定
    bool isChargingActive = !chargeGroups_.empty();
    if (!isChargingActive) {
        for (uint32_t i = 0; i < droneCount_; ++i) {
            if (dronesShadow_[i].hp > 0.0f && dronesShadow_[i].state == 1) {
                isChargingActive = true;
                break;
            }
        }
    }

    // 突撃中は本隊の左右往復移動タイマーを停止し、その場に留める
    if (!isChargingActive) {
        bossMoveTimer_ += deltaTime;
    }

    // 正面少し上（高さ+4.5m, 前方22m）を中心に、左・正面・右の3区間を左右に往復（最終フェーズ時は正面固定）
    float anchorLateral = bossPhaseState_ ? bossPhaseState_->ComputeAnchorLateral(bossMoveTimer_) : 0.0f;
    float anchorHeight = 4.5f + std::sin(bossMoveTimer_ * 1.2f) * 0.6f;
    float anchorDepth = 22.0f;
    Vector3 anchorWorld = MathUtils::Add(
        bossPlayerBasePos_,
        MathUtils::Add(
            MathUtils::Multiply(anchorLateral, right),
            MathUtils::Add(MathUtils::Multiply(anchorHeight, up), MathUtils::Multiply(anchorDepth, forward))
        )
    );

    float sumLocalX = 0.0f;
    int cruisingSampleCount = 0;

    for (uint32_t i = 0; i < droneCount_; ++i) {
        if (inActiveGroup[i]) continue;
        auto& self = dronesShadow_[i];
        if (self.hp <= 0.0f || self.state == 2) continue;

        if (self.state == 1) {
            // 第一形態の個別突撃ドローン
            self.stateTimer += deltaTime;
            if (self.stateTimer < 0.0f) {
                // 突撃開始ディレイ中はその場でプレイヤーを向く
                Vector3 toPlayer = MathUtils::Subtract(playerTarget, self.position);
                if (MathUtils::Length(toPlayer) > 0.001f) {
                    self.velocity = MathUtils::Multiply(2.0f, MathUtils::Normalize(toPlayer));
                }
                continue;
            }

            Vector3 toPlayer = MathUtils::Subtract(playerTarget, self.position);
            float distToPlayer = MathUtils::Length(toPlayer);
            if (distToPlayer < 2.2f) {
                if (isPlayerCovering) {
                    // カバー時（通常フェーズ）：爆発・自傷無し、プレイヤーダメージ無し、帰還状態にする
                    self.state = 4; // Returning
                    self.stateTimer = 0.0f;
                    self.velocity = MathUtils::Add(MathUtils::Multiply(8.0f, forward), { 0.0f, 9.0f, 0.0f });
                } else {
                    // 通常時（通常フェーズ）：直撃ダメージ10＆自傷（衝突機体撃破）
                    frameEvents_.selfDestructPositions.push_back(self.position);
                    frameEvents_.playerDirectHits += 1;
                    self.hp = 0.0f;
                    self.state = 2; // Dead
                }
                continue;
            }

            Vector3 dirToPlayer = MathUtils::Multiply(1.0f / distToPlayer, toPlayer);
            Vector3 desiredVel = MathUtils::Multiply(settings_.ChargeSpeed, dirToPlayer);
            self.velocity = MathUtils::Add(self.velocity, MathUtils::Multiply((std::min)(1.0f, deltaTime * 6.0f), MathUtils::Subtract(desiredVel, self.velocity)));
            self.position = MathUtils::Add(self.position, MathUtils::Multiply(deltaTime, self.velocity));
        } else if (self.state == 4) {
            // 帰還ドローン (state == 4: カバーされて本隊アンカーへ帰還中、突撃対象外)
            Vector3 toMainAnchor = MathUtils::Subtract(anchorWorld, self.position);
            float distToMain = MathUtils::Length(toMainAnchor);
            if (distToMain > 0.001f) {
                Vector3 returnVel = MathUtils::Multiply(settings_.maxSpeed * 1.35f, MathUtils::Multiply(1.0f / distToMain, toMainAnchor));
                self.velocity = MathUtils::Add(self.velocity, MathUtils::Multiply((std::min)(1.0f, deltaTime * 4.5f), MathUtils::Subtract(returnVel, self.velocity)));
            }
            self.position = MathUtils::Add(self.position, MathUtils::Multiply(deltaTime, self.velocity));

            Vector3 rel = MathUtils::Subtract(self.position, bossPlayerBasePos_);
            float localZ = MathUtils::Dot(rel, forward);
            const float minAllowedZ = (std::max)(17.5f, anchorDepth - 2.5f);
            if (distToMain < 5.0f && localZ >= minAllowedZ) {
                self.state = 0; // Cruise (巡航へ合流)
                self.stateTimer = 0.0f;
            }
        } else {
            // 巡航ドローン (state == 0: Boids + 左右往復アンカー追従)
            self.stateTimer += deltaTime;
            bool isEvading = (self.stateTimer < 0.0f);

            Vector3 separation = { 0.0f, 0.0f, 0.0f };
            Vector3 alignment = { 0.0f, 0.0f, 0.0f };
            Vector3 cohesion = { 0.0f, 0.0f, 0.0f };
            uint32_t neighborCount = 0;

            for (uint32_t j = 0; j < droneCount_; ++j) {
                if (i == j || inActiveGroup[j]) continue;
                const auto& other = dronesShadow_[j];
                if (other.hp <= 0.0f || other.state != 0) continue;

                Vector3 diff = MathUtils::Subtract(self.position, other.position);
                float dist = MathUtils::Length(diff);
                if (dist < settings_.neighborRadius && dist > 0.0001f) {
                    if (dist < settings_.separationDist) {
                        separation = MathUtils::Add(separation, MathUtils::Multiply((1.0f - dist / settings_.separationDist) / dist, diff));
                    }
                    alignment = MathUtils::Add(alignment, other.velocity);
                    cohesion = MathUtils::Add(cohesion, other.position);
                    neighborCount++;
                }
            }

            Vector3 accel = { 0.0f, 0.0f, 0.0f };
            if (neighborCount > 0) {
                float invN = 1.0f / static_cast<float>(neighborCount);
                alignment = MathUtils::Multiply(invN, alignment);
                if (MathUtils::Length(alignment) > 0.001f) {
                    alignment = MathUtils::Subtract(MathUtils::Multiply(settings_.maxSpeed, MathUtils::Normalize(alignment)), self.velocity);
                }
                cohesion = MathUtils::Multiply(invN, cohesion);
                Vector3 cohesionDir = MathUtils::Subtract(cohesion, self.position);
                if (MathUtils::Length(cohesionDir) > 0.001f) {
                    cohesion = MathUtils::Subtract(MathUtils::Multiply(settings_.maxSpeed, MathUtils::Normalize(cohesionDir)), self.velocity);
                }

                accel = MathUtils::Add(accel, MathUtils::Multiply(settings_.separationWeight * 8.0f, separation));
                if (!isEvading) {
                    accel = MathUtils::Add(accel, MathUtils::Multiply(settings_.alignmentWeight, alignment));
                    accel = MathUtils::Add(accel, MathUtils::Multiply(settings_.cohesionWeight * 0.8f, cohesion));
                }
            }

            // 左右往復アンカーへの誘導（回避中もアンカーから離れすぎないよう適度に引き戻す）
            Vector3 toAnchor = MathUtils::Subtract(anchorWorld, self.position);
            float distToAnchor = MathUtils::Length(toAnchor);
            if (distToAnchor > 0.001f) {
                float pullWeight = isEvading ? 1.5f : (isChargingActive ? 3.5f : 2.5f);
                Vector3 desiredSpeed = MathUtils::Multiply((std::min)(settings_.maxSpeed, distToAnchor * 2.2f), MathUtils::Multiply(1.0f / distToAnchor, toAnchor));
                Vector3 steer = MathUtils::Subtract(desiredSpeed, self.velocity);
                accel = MathUtils::Add(accel, MathUtils::Multiply(pullWeight, steer));
            }

            // 速度更新
            self.velocity = MathUtils::Add(self.velocity, MathUtils::Multiply(deltaTime, accel));

            // 突撃中は本隊の慣性による横流れを抑え、停止アンカー付近に留める
            if (isChargingActive && !isEvading) {
                float latVel = MathUtils::Dot(self.velocity, right);
                float toAnchorLat = MathUtils::Dot(toAnchor, right);
                if (std::abs(toAnchorLat) < 4.5f) {
                    self.velocity = MathUtils::Subtract(self.velocity, MathUtils::Multiply(latVel * 0.15f, right));
                }
            }

            float maxSpd = settings_.maxSpeed;
            float spd = MathUtils::Length(self.velocity);
            if (spd > maxSpd) {
                self.velocity = MathUtils::Multiply(maxSpd / spd, self.velocity);
            }

            self.position = MathUtils::Add(self.position, MathUtils::Multiply(deltaTime, self.velocity));

            // 本隊アンカー基準の相対最大距離制限（回避行動でも画面内に収まり、間隔が離れすぎないようにする）
            Vector3 rel = MathUtils::Subtract(self.position, bossPlayerBasePos_);
            float localX = MathUtils::Dot(rel, right);
            float localY = rel.y;
            float localZ = MathUtils::Dot(rel, forward);

            const float kMaxOffsetFromAnchorX = 5.5f;
            const float kMaxOffsetFromAnchorY = 2.4f;
            const float kMaxOffsetFromAnchorZ = 2.5f;

            const float minAllowedX = (std::max)(-20.5f, anchorLateral - kMaxOffsetFromAnchorX);
            const float maxAllowedX = (std::min)( 20.5f, anchorLateral + kMaxOffsetFromAnchorX);
            const float minAllowedY = (std::max)(  1.8f, anchorHeight  - kMaxOffsetFromAnchorY);
            const float maxAllowedY = (std::min)(  8.0f, anchorHeight  + kMaxOffsetFromAnchorY);
            const float minAllowedZ = (std::max)( 17.5f, anchorDepth   - kMaxOffsetFromAnchorZ);
            const float maxAllowedZ = (std::min)( 25.0f, anchorDepth   + kMaxOffsetFromAnchorZ);

            float vx = MathUtils::Dot(self.velocity, right);
            float vy = self.velocity.y;
            float vz = MathUtils::Dot(self.velocity, forward);

            if (localX < minAllowedX) { localX = minAllowedX; if (vx < 0.0f) vx = -vx * 0.3f; }
            if (localX > maxAllowedX) { localX = maxAllowedX; if (vx > 0.0f) vx = -vx * 0.3f; }
            if (localY < minAllowedY) { localY = minAllowedY; if (vy < 0.0f) vy = -vy * 0.3f; }
            if (localY > maxAllowedY) { localY = maxAllowedY; if (vy > 0.0f) vy = -vy * 0.3f; }
            if (localZ < minAllowedZ) { localZ = minAllowedZ; if (vz < 0.0f) vz = -vz * 0.3f; }
            if (localZ > maxAllowedZ) { localZ = maxAllowedZ; if (vz > 0.0f) vz = -vz * 0.3f; }

            self.position = MathUtils::Add(
                bossPlayerBasePos_,
                MathUtils::Add(
                    MathUtils::Multiply(localX, right),
                    MathUtils::Add(MathUtils::Multiply(localY, up), MathUtils::Multiply(localZ, forward))
                )
            );
            self.velocity = MathUtils::Add(
                MathUtils::Multiply(vx, right),
                MathUtils::Add(MathUtils::Multiply(vy, up), MathUtils::Multiply(vz, forward))
            );

            sumLocalX += localX;
            cruisingSampleCount++;
        }
    }

    // =========================================================
    // 5. カメラの3区間（左: -1, 正面: 0, 右: +1）切り替え判定
    // =========================================================
    // 突撃中（通常突撃・グループ突撃）および最終突撃発動後はカメラ区間を固定し、不要なカメラ切り替えを防ぐ
    if (!isChargingActive && (!bossPhaseState_ || bossPhaseState_->ShouldUpdateCameraSection())) {
        float refLocalX = anchorLateral;
        if (cruisingSampleCount > 0) {
            refLocalX = sumLocalX / static_cast<float>(cruisingSampleCount);
        } else if (!chargeGroups_.empty()) {
            Vector3 rel = MathUtils::Subtract(chargeGroups_[0].centerPos, bossPlayerBasePos_);
            refLocalX = MathUtils::Dot(rel, right);
        }

        // 境界でのガタつきを防ぐヒステリシス付き3区間判定
        if (currentSection_ == 0) {
            if (refLocalX > 7.0f) {
                currentSection_ = 1;  // 右区間へ切り替え
            } else if (refLocalX < -7.0f) {
                currentSection_ = -1; // 左区間へ切り替え
            }
        } else if (currentSection_ == 1) {
            if (refLocalX < 5.0f) {
                currentSection_ = 0;  // 正面区間へ戻る
            }
        } else if (currentSection_ == -1) {
            if (refLocalX > -5.0f) {
                currentSection_ = 0;  // 正面区間へ戻る
            }
        }
    }
}

float SwarmManager::GetTargetCameraYaw() const {
    if (!isBossMode_) return bossBaseYaw_;
    if (bossPhaseState_) {
        return bossPhaseState_->GetTargetCameraYaw(*this);
    }
    const float kSectionYawStep = 0.48f;
    return bossBaseYaw_ + static_cast<float>(currentSection_) * kSectionYawStep;
}

float SwarmManager::GetTargetCameraPitch() const {
    if (!isBossMode_) return 0.0f;
    if (bossPhaseState_) {
        return bossPhaseState_->GetTargetCameraPitch(*this);
    }
    return -0.18f;
}

bool SwarmManager::IsRayHitting(const Vector3& rayOrigin, const Vector3& rayDir) const {
    if (!isActive_) return false;

    for (const auto& group : chargeGroups_) {
        if (group.isDead) continue;
        Vector3 toCenter = MathUtils::Subtract(group.centerPos, rayOrigin);
        float t = MathUtils::Dot(toCenter, rayDir);
        if (t > 0.0f) {
            Vector3 closestPt = MathUtils::Add(rayOrigin, MathUtils::Multiply(t, rayDir));
            if (MathUtils::Length(MathUtils::Subtract(group.centerPos, closestPt)) <= group.hitRadius) {
                return true;
            }
        }
    }

    for (uint32_t i = 0; i < droneCount_; ++i) {
        const auto& d = dronesShadow_[i];
        if (d.hp <= 0.0f || d.state == 2) continue;
        Vector3 toDrone = MathUtils::Subtract(d.position, rayOrigin);
        float t = MathUtils::Dot(toDrone, rayDir);
        if (t > 0.0f) {
            Vector3 closestPt = MathUtils::Add(rayOrigin, MathUtils::Multiply(t, rayDir));
            float hitRad = (d.state == 1) ? 1.8f : 1.5f;
            if (MathUtils::Length(MathUtils::Subtract(d.position, closestPt)) <= hitRad) {
                return true;
            }
        }
    }
    return false;
}

bool SwarmManager::GetAutoAimTarget(Vector3& outTargetPos) const {
    if (!isActive_) return false;

    auto calcDistSq = [this](const Vector3& pos) {
        Vector3 diff = MathUtils::Subtract(pos, bossPlayerBasePos_);
        return MathUtils::Dot(diff, diff);
    };

    // 1. アクティブな突撃グループ（最もプレイヤーに近いもの）
    bool foundGroup = false;
    float minGroupDistSq = (std::numeric_limits<float>::max)();
    for (const auto& group : chargeGroups_) {
        if (group.isDead || group.hp <= 0) continue;
        float dSq = calcDistSq(group.centerPos);
        if (dSq < minGroupDistSq) {
            minGroupDistSq = dSq;
            outTargetPos = group.centerPos;
            foundGroup = true;
        }
    }
    if (foundGroup) {
        return true;
    }

    // 2. 個別突撃中のドローン（最もプレイヤーに近いもの）
    bool foundChargeDrone = false;
    float minChargeDistSq = (std::numeric_limits<float>::max)();
    for (uint32_t i = 0; i < droneCount_; ++i) {
        const auto& d = dronesShadow_[i];
        if (d.hp > 0.0f && d.state == 1) {
            float dSq = calcDistSq(d.position);
            if (dSq < minChargeDistSq) {
                minChargeDistSq = dSq;
                outTargetPos = d.position;
                foundChargeDrone = true;
            }
        }
    }
    if (foundChargeDrone) {
        return true;
    }

    // 3. 巡航中のドローン（最もプレイヤーに近いもの）
    bool foundCruiseDrone = false;
    float minCruiseDistSq = (std::numeric_limits<float>::max)();
    for (uint32_t i = 0; i < droneCount_; ++i) {
        const auto& d = dronesShadow_[i];
        if (d.hp > 0.0f && (d.state == 0 || d.state == 4)) {
            float dSq = calcDistSq(d.position);
            if (dSq < minCruiseDistSq) {
                minCruiseDistSq = dSq;
                outTargetPos = d.position;
                foundCruiseDrone = true;
            }
        }
    }
    return foundCruiseDrone;
}

int SwarmManager::GetBossPhase() const {
    return bossPhaseState_ ? bossPhaseState_->GetPhaseNumber() : 1;
}

void SwarmManager::UpdateHpGauges(Camera* camera, float clientWidth, float clientHeight) {
    if (!isActive_ || !camera) return;

    Vector3 camPos = camera->GetTranslate();
    Vector3 camRot = camera->GetRotate();
    Vector3 camForward = {
        std::cos(camRot.x) * std::sin(camRot.y),
        -std::sin(camRot.x),
        std::cos(camRot.x) * std::cos(camRot.y)
    };
    Matrix4x4 vp = camera->GetViewProjectionMatrix();

    for (auto& group : chargeGroups_) {
        group.isHpGaugeVisible = false;
        if (group.isDead || group.maxHp <= 0) continue;

        float yOffset = group.isFinalGroup ? 2.2f : 1.7f;
        Vector3 headWorldPos = MathUtils::Add(group.centerPos, { 0.0f, yOffset, 0.0f });
        Vector3 toHead = MathUtils::Subtract(headWorldPos, camPos);
        if (MathUtils::Dot(toHead, camForward) <= 0.1f) continue;

        Vector3 ndc = MathUtils::TransformPoint(headWorldPos, vp);
        if (ndc.z < 0.0f || ndc.z > 1.0f) continue;

        float screenX = (ndc.x * 0.5f + 0.5f) * clientWidth;
        float screenY = (1.0f - (ndc.y * 0.5f + 0.5f)) * clientHeight;

        float targetRatio = static_cast<float>(group.hp) / static_cast<float>(group.maxHp);
        if (targetRatio < 0.0f) targetRatio = 0.0f;
        if (group.damageBarRatio > targetRatio) {
            group.damageBarRatio -= (group.damageBarRatio - targetRatio) * 0.12f;
            if (group.damageBarRatio - targetRatio < 0.005f) {
                group.damageBarRatio = targetRatio;
            }
        } else {
            group.damageBarRatio = targetRatio;
        }

        const float kBarFullWidth = group.isFinalGroup ? 110.0f : 78.0f;
        const float kBarHeight = group.isFinalGroup ? 11.0f : 8.0f;
        float barLeftX = screenX - kBarFullWidth * 0.5f;

        if (group.hpBgSprite) {
            group.hpBgSprite->SetTranslate({ barLeftX, screenY });
            group.hpBgSprite->SetScale({ kBarFullWidth, kBarHeight });
            group.hpBgSprite->Update();
        }
        if (group.hpDamageSprite) {
            group.hpDamageSprite->SetTranslate({ barLeftX, screenY });
            group.hpDamageSprite->SetScale({ kBarFullWidth * group.damageBarRatio, kBarHeight });
            group.hpDamageSprite->Update();
        }
        if (group.hpFillSprite) {
            group.hpFillSprite->SetTranslate({ barLeftX, screenY });
            group.hpFillSprite->SetScale({ kBarFullWidth * targetRatio, kBarHeight });
            if (targetRatio <= 0.35f) {
                group.hpFillSprite->SetColor({ 1.0f, 0.35f, 0.1f, 1.0f });
            } else {
                group.hpFillSprite->SetColor({ 1.0f, 0.85f, 0.1f, 1.0f });
            }
            group.hpFillSprite->Update();
        }
        if (group.hpFrameSprite) {
            group.hpFrameSprite->SetTranslate({ screenX, screenY });
            group.hpFrameSprite->SetScale({ kBarFullWidth + 10.0f, kBarHeight + 8.0f });
            group.hpFrameSprite->Update();
        }

        group.isHpGaugeVisible = true;
    }
}

void SwarmManager::DrawHpGauges() {
    if (!isActive_) return;
    for (const auto& group : chargeGroups_) {
        if (!group.isDead && group.isHpGaugeVisible) {
            if (group.hpBgSprite) group.hpBgSprite->Draw();
            if (group.hpDamageSprite) group.hpDamageSprite->Draw();
            if (group.hpFillSprite) group.hpFillSprite->Draw();
            if (group.hpFrameSprite) group.hpFrameSprite->Draw();
        }
    }
}

void SwarmManager::Draw(Camera* camera) {
    (void)camera;
    if (!isActive_ || !droneModel_ || droneCount_ == 0) return;

    auto dxContext = DX12Context::GetInstance();
    auto srvManager = SrvManager::GetInstance();
    auto commandList = dxContext->GetCommandList();

    // CPU側の最新ドローン状態 (dronesShadow_) をGPUバッファへ同期転送
    if (pendingSpawn_ || needGpuSync_) {
        void* mappedPtr = nullptr;
        size_t bufferSize = sizeof(Swarm::DroneData) * droneCount_;
        intermediateUploadBuffer_->Map(0, nullptr, &mappedPtr);
        std::memcpy(mappedPtr, dronesShadow_.data(), bufferSize);
        intermediateUploadBuffer_->Unmap(0, nullptr);

        D3D12_RESOURCE_BARRIER bCopy{};
        bCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        bCopy.Transition.pResource = droneBuffers_[readIndex_].Get();
        bCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        bCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        bCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &bCopy);

        commandList->CopyBufferRegion(droneBuffers_[readIndex_].Get(), 0, intermediateUploadBuffer_.Get(), 0, bufferSize);

        bCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        bCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        commandList->ResourceBarrier(1, &bCopy);

        pendingSpawn_ = false;
        needGpuSync_ = false;
    }

    // GPU Instancing 描画発行
    srvManager->PreDraw();

    commandList->SetGraphicsRootSignature(drawRootSignature_.Get());
    commandList->SetPipelineState(drawPSO_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW vbv = droneModel_->GetVertexBufferView();
    D3D12_INDEX_BUFFER_VIEW ibv = droneModel_->GetIndexBufferView();
    commandList->IASetVertexBuffers(0, 1, &vbv);
    commandList->IASetIndexBuffer(&ibv);

    // t0: StructuredBuffer
    srvManager->SetGraphicsRootDescriptorTable(0, srvIndices_[readIndex_]);

    // b1: CameraTransform
    commandList->SetGraphicsRootConstantBufferView(1, cameraConstantBuffer_->GetGPUVirtualAddress());

    // t1: Texture
    srvManager->SetGraphicsRootDescriptorTable(2, textureSrvIndex_);

    // t2: Dissolve Mask Texture
    srvManager->SetGraphicsRootDescriptorTable(3, dissolveMaskSrvIndex_);

    commandList->DrawIndexedInstanced(droneModel_->GetIndexCount(), droneCount_, 0, 0, 0);
}

void SwarmManager::SpawnDrones(uint32_t count, const Vector3 & centerPos, float radius) {
    uint32_t actualCount = (count <= droneCount_) ? count : droneCount_;

    isBossMode_ = false;
    chargeGroups_.clear();

    settings_.boundsMin = { centerPos.x - radius, centerPos.y - 10.0f, centerPos.z - radius };
    settings_.boundsMax = { centerPos.x + radius, centerPos.y + 12.0f, centerPos.z + radius };
    settings_.neighborRadius = 8.0f;
    settings_.separationDist = 2.5f;
    settings_.attackDistance = radius * 0.9f;
    settings_.maxSpeed = 15.0f;
    settings_.ChargeSpeed = 22.0f;
    settings_.droneCount = actualCount;

    pendingSpawnData_.resize(droneCount_);
    std::mt19937 mt(42);
    std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> distDist(6.0f, radius * 0.95f);
    std::uniform_real_distribution<float> heightDist(-3.0f, 6.0f);
    std::uniform_real_distribution<float> speedDist(8.0f, 13.0f);

    for (uint32_t i = 0; i < droneCount_; ++i) {
        if (i < actualCount) {
            float angle = angleDist(mt);
            float dist = distDist(mt);
            float h = heightDist(mt);
            float spd = speedDist(mt);

            pendingSpawnData_[i].position = {
                centerPos.x + std::cos(angle) * dist,
                centerPos.y + h,
                centerPos.z + std::sin(angle) * dist
            };
            pendingSpawnData_[i].velocity = {
                -std::sin(angle) * spd,
                (i % 2 == 0 ? 1.0f : -1.0f) * 1.5f,
                std::cos(angle) * spd
            };
            pendingSpawnData_[i].hp = 10.0f;
            pendingSpawnData_[i].state = 0; // Cruise
            pendingSpawnData_[i].target = centerPos;
            pendingSpawnData_[i].stateTimer = static_cast<float>(i % 30) * 0.1f;
        } else {
            pendingSpawnData_[i].position = { 0.0f, -999.0f, 0.0f };
            pendingSpawnData_[i].velocity = { 0.0f, 0.0f, 0.0f };
            pendingSpawnData_[i].hp = 0.0f;
            pendingSpawnData_[i].state = 2; // Dead
            pendingSpawnData_[i].target = { 0.0f, 0.0f, 0.0f };
            pendingSpawnData_[i].stateTimer = 0.0f;
        }
    }

    pendingSpawn_ = true;
    needGpuSync_ = true;
    isActive_ = true;
    dronesShadow_ = pendingSpawnData_;
}

uint32_t SwarmManager::GetAliveDroneCount() const {
    if (!isActive_) return 0;
    uint32_t count = 0;
    for (const auto& d : dronesShadow_) {
        if (d.hp > 0.0f && d.state != 2) {
            count++;
        }
    }
    return count;
}

void SwarmManager::UpdateCrashingDrones(float deltaTime) {
    if (crashSleeping_.size() != droneCount_) {
        crashSleeping_.assign(droneCount_, false);
    }

    const float droneRadius = droneScale_ * crashCollisionRadiusScale_;
    const float groundY = bossGroundY_ + droneRadius;
    const float kGravity = -18.0f;
    const float airDrag = 0.99f;
    const float groundFriction = crashGroundFriction_;
    const float bounciness = crashBounciness_;
    const float pushStrength = crashPushStrength_;
    const float sleepThresholdSq = 0.05f * 0.05f;

    std::vector<uint32_t> crashingIndices;
    crashingIndices.reserve(droneCount_);

    auto freezeDirection = [](Swarm::DroneData& d) {
        // VS側で急に向きが変わらないよう、最後の水平向きを極小ベクトルとして保持
        float hLen = std::sqrt(d.velocity.x * d.velocity.x + d.velocity.z * d.velocity.z);
        if (hLen > 0.0001f) {
            d.velocity = { (d.velocity.x / hLen) * 0.002f, 0.0f, (d.velocity.z / hLen) * 0.002f };
        } else {
            d.velocity = { 0.002f, 0.0f, 0.0f };
        }
    };

    // 1. 重力・移動・地面との摩擦処理
    for (uint32_t i = 0; i < droneCount_; ++i) {
        auto& d = dronesShadow_[i];
        if (d.state != 3) continue;

        crashingIndices.push_back(i);
        d.stateTimer += deltaTime;

        if (crashSleeping_[i]) continue;

        // 重力の適用
        d.velocity.y += kGravity * deltaTime;

        // 空気抵抗の適用
        d.velocity.x *= airDrag;
        d.velocity.z *= airDrag;

        // 座標の更新
        d.position = MathUtils::Add(d.position, MathUtils::Multiply(deltaTime, d.velocity));

        // 地面との当たり判定
        if (d.position.y <= groundY) {
            d.position.y = groundY;

            // 地面でのバウンド処理
            if (d.velocity.y < 0.0f) {
                if (std::abs(d.velocity.y) > 1.0f) {
                    d.velocity.y = -d.velocity.y * bounciness;
                } else {
                    d.velocity.y = 0.0f; // 小さなバウンドは止める
                }
            }

            // 地面との摩擦（水平速度を強く減衰）
            d.velocity.x *= groundFriction;
            d.velocity.z *= groundFriction;

            // 速度が十分小さくなったら完全停止（スリープ）にする
            float speedSq = d.velocity.x * d.velocity.x + d.velocity.y * d.velocity.y + d.velocity.z * d.velocity.z;
            if (speedSq < sleepThresholdSq && d.velocity.y == 0.0f) {
                freezeDirection(d);
                crashSleeping_[i] = true;
            }
        }
    }

    // 2. ドローン同士の球判定（重なり防止の押し出し・プルプル振動抑制）
    const float minDist = droneRadius * 2.0f;
    const float minDistSq = minDist * minDist;

    for (size_t a = 0; a < crashingIndices.size(); ++a) {
        uint32_t idxA = crashingIndices[a];
        auto& dA = dronesShadow_[idxA];

        for (size_t b = a + 1; b < crashingIndices.size(); ++b) {
            uint32_t idxB = crashingIndices[b];
            auto& dB = dronesShadow_[idxB];

            // 両方とも完全停止（スリープ）していれば判定不要
            if (crashSleeping_[idxA] && crashSleeping_[idxB]) continue;

            Vector3 diff = MathUtils::Subtract(dB.position, dA.position);
            float distSq = MathUtils::Dot(diff, diff);

            if (distSq < minDistSq) {
                float dist = std::sqrt(distSq);
                Vector3 normal = (dist > 0.0001f) ? MathUtils::Multiply(1.0f / dist, diff) : Vector3{ 1.0f, 0.0f, 0.0f };
                float overlap = (minDist - dist) * 0.5f * pushStrength;

                if (!crashSleeping_[idxA]) {
                    float scaleA = crashSleeping_[idxB] ? 2.0f : 1.0f;
                    dA.position.x -= normal.x * overlap * scaleA;
                    dA.position.y = (std::max)(groundY, dA.position.y - normal.y * overlap * scaleA);
                    dA.position.z -= normal.z * overlap * scaleA;

                    // スリープ済み残骸の上に乗り上げて支えられている場合は落下停止・摩擦適用してスリープ
                    if (crashSleeping_[idxB] && -normal.y > 0.45f) {
                        if (dA.velocity.y < 0.0f) dA.velocity.y = 0.0f;
                        dA.velocity.x *= groundFriction;
                        dA.velocity.z *= groundFriction;
                        float speedSq = dA.velocity.x * dA.velocity.x + dA.velocity.y * dA.velocity.y + dA.velocity.z * dA.velocity.z;
                        if (speedSq < sleepThresholdSq) {
                            freezeDirection(dA);
                            crashSleeping_[idxA] = true;
                        }
                    }
                }
                if (!crashSleeping_[idxB]) {
                    float scaleB = crashSleeping_[idxA] ? 2.0f : 1.0f;
                    dB.position.x += normal.x * overlap * scaleB;
                    dB.position.y = (std::max)(groundY, dB.position.y + normal.y * overlap * scaleB);
                    dB.position.z += normal.z * overlap * scaleB;

                    // スリープ済み残骸の上に乗り上げて支えられている場合は落下停止・摩擦適用してスリープ
                    if (crashSleeping_[idxA] && normal.y > 0.45f) {
                        if (dB.velocity.y < 0.0f) dB.velocity.y = 0.0f;
                        dB.velocity.x *= groundFriction;
                        dB.velocity.z *= groundFriction;
                        float speedSq = dB.velocity.x * dB.velocity.x + dB.velocity.y * dB.velocity.y + dB.velocity.z * dB.velocity.z;
                        if (speedSq < sleepThresholdSq) {
                            freezeDirection(dB);
                            crashSleeping_[idxB] = true;
                        }
                    }
                }
            }
        }
    }
}

bool SwarmManager::GetCrashCenter(Vector3& outCenter) const {
    Vector3 sum = { 0.0f, 0.0f, 0.0f };
    int count = 0;
    for (uint32_t i = 0; i < droneCount_; ++i) {
        const auto& d = dronesShadow_[i];
        if (d.state == 3) {
            sum = MathUtils::Add(sum, d.position);
            count++;
        }
    }
    if (count > 0) {
        outCenter = MathUtils::Multiply(1.0f / static_cast<float>(count), sum);
        return true;
    }

    // 万一残骸がない場合はボス戦正面の地面座標を返す
    Vector3 forward = { std::sin(bossBaseYaw_), 0.0f, std::cos(bossBaseYaw_) };
    outCenter = MathUtils::Add(bossPlayerBasePos_, MathUtils::Multiply(14.0f, forward));
    outCenter.y = bossGroundY_;
    return false;
}
