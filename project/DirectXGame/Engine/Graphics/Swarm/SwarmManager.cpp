#include "SwarmManager.h"
#include "Base/DX12Context.h"
#include "Base/SrvManager.h"
#include "PSO/PipelineManager.h"
#include "Texture/TextureManager.h"
#include "Logger/Logger.h"

#include <random>
#include <cassert>

using namespace Microsoft::WRL;

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
    settings_.kamikazeSpeed = 30.0f;
    settings_.droneCount = droneCount_;
    settings_.rayRadius = 2.0f;
    settings_.attackDamage = 0.0f;

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

    commandList->CopyResource(droneBuffers_[0].Get(), intermediateUploadBuffer_.Get());
    commandList->CopyResource(droneBuffers_[1].Get(), intermediateUploadBuffer_.Get());

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
        // ルートパラメータ設計:
        // Param 0: CBV (b0: SwarmGlobalConstants)
        // Param 1: SRV Table (t0: gPrevDrones)
        // Param 2: UAV Table (u0: gNextDrones)
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
        // CBV
        rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[0].Descriptor.ShaderRegister = 0;

        // SRV Table
        rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[1].DescriptorTable.pDescriptorRanges = &srvRange;

        // UAV Table
        rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[2].DescriptorTable.pDescriptorRanges = &uavRange;

        D3D12_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.NumParameters = _countof(rootParams);
        rsDesc.pParameters = rootParams;
        rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        computeRootSignature_ = pipelineManager->CreateComputeRootSignature(rsDesc);

        // シェーダーコンパイル & PSO生成
        auto csBlob = dxContext->CompileShader(L"Resources/Shaders/Swarm/SwarmSimulate.CS.hlsl", L"cs_6_0");
        computePSO_ = pipelineManager->CreateComputePSO(computeRootSignature_.Get(), csBlob.Get());
    }

    // ==========================================
    // 2. Draw Pipeline (SwarmDraw.VS / PS)
    // ==========================================
    {
        // ルートパラメータ設計:
        // Param 0: SRV Table (t0: gDrones StructuredBuffer) - VS用
        // Param 1: CBV (b1: CameraTransform) - VS用
        // Param 2: SRV Table (t1: Texture2D) - PS用
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

        D3D12_ROOT_PARAMETER rootParams[3]{};
        // t0: StructuredBuffer
        rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[0].DescriptorTable.pDescriptorRanges = &droneRange;

        // b1: Camera CBV
        rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParams[1].Descriptor.ShaderRegister = 1;

        // t1: Texture
        rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[2].DescriptorTable.pDescriptorRanges = &texRange;

        // Static Sampler (s0)
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

        // シェーダーコンパイル
        auto vsBlob = dxContext->CompileShader(L"Resources/Shaders/Swarm/SwarmDraw.VS.hlsl", L"vs_6_0");
        auto psBlob = dxContext->CompileShader(L"Resources/Shaders/Swarm/SwarmDraw.PS.hlsl", L"ps_6_0");

        // Input Layout
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

        // ブレンド (不透明)
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        // ラスタライザ (裏面カリングによる不可視化を防止)
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

        // 深度
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
}

void SwarmManager::Update(float deltaTime,
                          const Vector3 & playerPos,
                          Camera* camera,
                          const AttackCommand* attackCmd) {
    if (!isActive_) {
        return;
    }

    // 1. 定数バッファの更新 (CPU Mappingポインタへの書き込みのみ、コマンドリスト不要)
    settings_.playerPosition = playerPos;
    settings_.deltaTime = deltaTime;
    settings_.droneCount = droneCount_;

    if (camera) {
        Vector3 camT = camera->GetTranslate();
        settings_.cameraPosition = { camT.x, camT.y, camT.z };

        Matrix4x4 vp = camera->GetViewProjectionMatrix();
        std::memcpy(&mappedCameraConstants_->viewProjection, &vp, sizeof(DirectX::XMFLOAT4X4));
        mappedCameraConstants_->cameraPosition = settings_.cameraPosition;
        mappedCameraConstants_->droneScale = droneScale_;
    }

    if (attackCmd) {
        settings_.rayOrigin = attackCmd->rayOrigin;
        settings_.rayDirection = attackCmd->rayDirection;
        settings_.rayRadius = attackCmd->rayRadius;
        settings_.attackDamage = attackCmd->damage;
    } else {
        settings_.attackDamage = 0.0f;
    }

    *mappedConstants_ = settings_;
}

void SwarmManager::Draw(Camera* camera) {
    if (!isActive_ || !droneModel_ || droneCount_ == 0) return;

    auto dxContext = DX12Context::GetInstance();
    auto srvManager = SrvManager::GetInstance();
    auto commandList = dxContext->GetCommandList();

    // 1. スポーン予約がある場合は、両方のバッファへアップロード転送して状態を同期 (描画パス内なのでコマンドリストはOpen)
    if (pendingSpawn_) {
        void* mappedPtr = nullptr;
        size_t bufferSize = sizeof(Swarm::DroneData) * droneCount_;
        intermediateUploadBuffer_->Map(0, nullptr, &mappedPtr);
        std::memcpy(mappedPtr, pendingSpawnData_.data(), bufferSize);
        intermediateUploadBuffer_->Unmap(0, nullptr);

        // 両方のバッファを COPY_DEST に遷移
        D3D12_RESOURCE_BARRIER bCopies[2]{};
        for (int i = 0; i < 2; ++i) {
            bCopies[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            bCopies[i].Transition.pResource = droneBuffers_[i].Get();
            bCopies[i].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            bCopies[i].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            bCopies[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }
        commandList->ResourceBarrier(2, bCopies);

        commandList->CopyResource(droneBuffers_[0].Get(), intermediateUploadBuffer_.Get());
        commandList->CopyResource(droneBuffers_[1].Get(), intermediateUploadBuffer_.Get());

        for (int i = 0; i < 2; ++i) {
            bCopies[i].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            bCopies[i].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        }
        commandList->ResourceBarrier(2, bCopies);

        readIndex_ = 0;
        writeIndex_ = 1;
        dronesShadow_ = pendingSpawnData_;
        pendingSpawn_ = false;
    }

    // 2. Compute Shader シミュレーション実行
    // writeIndex_ のバッファを UNORDERED_ACCESS に遷移
    D3D12_RESOURCE_BARRIER preBarriers[2]{};
    preBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    preBarriers[0].Transition.pResource = droneBuffers_[readIndex_].Get();
    preBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    preBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; // そのまま
    preBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    preBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    preBarriers[1].Transition.pResource = droneBuffers_[writeIndex_].Get();
    preBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    preBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    preBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &preBarriers[1]);

    // 3. Compute Dispatch
    srvManager->PreCompute();
    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(computePSO_.Get());

    commandList->SetComputeRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());
    srvManager->SetComputeRootDescriptorTable(1, srvIndices_[readIndex_]);
    srvManager->SetComputeRootDescriptorTable(2, uavIndices_[writeIndex_]);

    uint32_t threadGroupX = (droneCount_ + 255) / 256;
    commandList->Dispatch(threadGroupX, 1, 1);

    // 4. UAV バリア (書き込み完了待ち)
    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = droneBuffers_[writeIndex_].Get();
    commandList->ResourceBarrier(1, &uavBarrier);

    // 5. 書き込み完了バッファを描画用 (NON_PIXEL_SHADER_RESOURCE) に遷移
    D3D12_RESOURCE_BARRIER postBarrier{};
    postBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    postBarrier.Transition.pResource = droneBuffers_[writeIndex_].Get();
    postBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    postBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    postBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &postBarrier);

    // 6. Ping-Pong スワップ (直前に書き込んだバッファが次回の読み取りSRVになる)
    std::swap(readIndex_, writeIndex_);

    // 7. GPU Instancing 描画発行
    srvManager->PreDraw();

    commandList->SetGraphicsRootSignature(drawRootSignature_.Get());
    commandList->SetPipelineState(drawPSO_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // メッシュの頂点・インデックスバッファ
    D3D12_VERTEX_BUFFER_VIEW vbv = droneModel_->GetVertexBufferView();
    D3D12_INDEX_BUFFER_VIEW ibv = droneModel_->GetIndexBufferView();
    commandList->IASetVertexBuffers(0, 1, &vbv);
    commandList->IASetIndexBuffer(&ibv);

    // t0: StructuredBuffer (直前にシミュレーション結果が書き込まれたバッファ = readIndex_)
    srvManager->SetGraphicsRootDescriptorTable(0, srvIndices_[readIndex_]);

    // b1: CameraTransform
    commandList->SetGraphicsRootConstantBufferView(1, cameraConstantBuffer_->GetGPUVirtualAddress());

    // t1: Texture
    srvManager->SetGraphicsRootDescriptorTable(2, textureSrvIndex_);

    // GPU Instancing 描画発行
    commandList->DrawIndexedInstanced(droneModel_->GetIndexCount(), droneCount_, 0, 0, 0);
}

void SwarmManager::SpawnDrones(uint32_t count, const Vector3 & centerPos, float radius) {
    uint32_t actualCount = (count <= droneCount_) ? count : droneCount_;

    // パラメータをプレイヤー周辺radius以内の移動に設定
    settings_.boundsMin = { centerPos.x - radius, centerPos.y - 10.0f, centerPos.z - radius };
    settings_.boundsMax = { centerPos.x + radius, centerPos.y + 12.0f, centerPos.z + radius };
    settings_.neighborRadius = 8.0f;
    settings_.separationDist = 2.5f;
    settings_.attackDistance = radius * 0.9f;
    settings_.maxSpeed = 15.0f;
    settings_.kamikazeSpeed = 22.0f;
    settings_.droneCount = actualCount;

    // スポーンデータの生成
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

            // プレイヤー中心の周囲に配置
            pendingSpawnData_[i].position = {
                centerPos.x + std::cos(angle) * dist,
                centerPos.y + h,
                centerPos.z + std::sin(angle) * dist
            };
            // プレイヤー周囲を旋回するような初期速度 (接線方向)
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
