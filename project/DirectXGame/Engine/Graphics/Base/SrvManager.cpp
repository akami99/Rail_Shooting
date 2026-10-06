#include "SrvManager.h"

#include "Base/DX12Context.h"

using namespace Microsoft::WRL;

const uint32_t SrvManager::kMaxSRVCount = 512;

std::unique_ptr<SrvManager> SrvManager::instance_ = nullptr;

// ★追加: シングルトンインスタンスの実装
SrvManager* SrvManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = std::make_unique<SrvManager>(Token{});
	}
	return instance_.get();
}

SrvManager::SrvManager(Token) {
	// コンストラクタ
}

// 初期化
void SrvManager::Initialize() {
	// デスクリプタヒープの生成
	descriptorHeap_ = DX12Context::GetInstance()->CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
	descriptorHeap_->SetName(L"SrvManager_DescriptorHeap");
	// デスクリプタヒープ1個分のサイズを取得して記録
	descriptorSize_ = DX12Context::GetInstance()->GetDevice()->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

// 終了
void SrvManager::Finalize() {
	descriptorHeap_.Reset();

	// シングルトンインスタンスの解放
	instance_.reset();
}

// 描画開始前処理
void SrvManager::PreDraw() {
	// 描画用のDescriptorHeapの設定
	// "生のポインタ配列"を作成(ComPtrだとスマートポインタなので変えないように！)
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap_.Get() };

	// 生のポインタ配列をSetDescriptorHeapsに渡す
	DX12Context::GetInstance()->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
}

bool SrvManager::AllocatableTexture() {
	// useIndexがkMaxSRVCount未満であることを確認
	if (useIndex_ < kMaxSRVCount) {
		return true;
	}

	assert(useIndex_ < kMaxSRVCount && "Error: texture use index out of bounds!");
	return false;
}

uint32_t SrvManager::Allocate() {
	// useIndexがkMaxSRVCount未満であることを確認
	assert(useIndex_ < kMaxSRVCount && "Error: texture use index out of bounds!");

	// returnする番号を一旦記録しておく
	uint32_t index = useIndex_;
	// 次回のために番号を1進める
	useIndex_++;
	// 記録した番号をreturn
	return index;
}

// SRVをセット
void SrvManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex,
	uint32_t srvIndex) {
	DX12Context::GetInstance()->GetCommandList()->SetGraphicsRootDescriptorTable(
		RootParameterIndex, GetGPUDescriptorHandle(srvIndex));
}

// Compute用にSRV/UAVをセット
void SrvManager::SetComputeRootDescriptorTable(UINT RootParameterIndex,
	uint32_t descriptorIndex) {
	DX12Context::GetInstance()->GetCommandList()->SetComputeRootDescriptorTable(
		RootParameterIndex, GetGPUDescriptorHandle(descriptorIndex));
}

// コンピュート前処理（デスクリプタヒープの設定）
void SrvManager::PreCompute() {
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap_.Get() };
	DX12Context::GetInstance()->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
}

// SRVの指定したインデックスのCPUディスクリプタハンドルを取得
D3D12_CPU_DESCRIPTOR_HANDLE
SrvManager::GetCPUDescriptorHandle(uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU =
		descriptorHeap_.Get()->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize_ * index);
	return handleCPU;
}

// SRVの指定したインデックスのGPUディスクリプタハンドルを取得
D3D12_GPU_DESCRIPTOR_HANDLE
SrvManager::GetGPUDescriptorHandle(uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU =
		descriptorHeap_.Get()->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize_ * index);
	return handleGPU;
}

// テクスチャ用のSRVを作成
void SrvManager::CreateSRVForTexture(uint32_t srvIndex,
	ComPtr<ID3D12Resource> resource,
	DXGI_FORMAT Format,
	UINT MipLevels, bool isCubeMap) {

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// CubeMapかどうかでViewDimensionを切り替える
	if (isCubeMap) {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE; // CubeMap
		srvDesc.TextureCube.MostDetailedMip = 0; // 最も詳細なミップレベル
		srvDesc.TextureCube.MipLevels = UINT_MAX; // ミップレベル数
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f; // 最小LODクランプ
	}
	else {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
		srvDesc.Texture2D.MipLevels = MipLevels;
	}

	// SRVを作成するディスクリプタヒープの場所を取得
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU = GetCPUDescriptorHandle(srvIndex);

	// SRVの生成
	DX12Context::GetInstance()->GetDevice()->CreateShaderResourceView(resource.Get(), &srvDesc,
		srvHandleCPU);
}

// StructuredBuffer用のSRVを作成
void SrvManager::CreateSRVForStructuredBuffer(uint32_t srvIndex,
	ComPtr<ID3D12Resource> resource,
	uint32_t numElement,
	uint32_t structureByteStride) {
	// SRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN; // StructuredBufferの場合はUNKNOWN
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER; // バッファとして扱う
	srvDesc.Buffer.FirstElement = 0;                    // 最初の要素位置
	srvDesc.Buffer.NumElements = numElement;            // 要素数
	srvDesc.Buffer.StructureByteStride =
		structureByteStride; // 構造体（要素）のバイトサイズ
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE; // 特にフラグは無し

	// SRVを作成するディスクリプタヒープの場所を取得
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU = GetCPUDescriptorHandle(srvIndex);

	// SRVの生成
	DX12Context::GetInstance()->GetDevice()->CreateShaderResourceView(resource.Get(), &srvDesc,
		srvHandleCPU);
}

// StructuredBuffer用のUAVを作成
void SrvManager::CreateUAVForStructuredBuffer(uint32_t uavIndex,
	ComPtr<ID3D12Resource> resource,
	uint32_t numElement,
	uint32_t structureByteStride) {
	// UAVの設定
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = numElement;
	uavDesc.Buffer.StructureByteStride = structureByteStride;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	// UAVを作成するディスクリプタヒープの場所を取得
	D3D12_CPU_DESCRIPTOR_HANDLE uavHandleCPU = GetCPUDescriptorHandle(uavIndex);

	// UAVの生成
	DX12Context::GetInstance()->GetDevice()->CreateUnorderedAccessView(
		resource.Get(), nullptr, &uavDesc, uavHandleCPU);
}
