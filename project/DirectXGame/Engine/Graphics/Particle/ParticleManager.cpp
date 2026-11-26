#include "ParticleManager.h"
#include "Base/DX12Context.h"
#include "Base/SrvManager.h"
#include "BlendMode/BlendMode.h"
#include "Camera/Camera.h"
#include "Logger/Logger.h"
#include "MathUtils.h"
#include "MatrixGenerators.h"
#include "PSO/PipelineManager.h"
#include "Texture/TextureManager.h"
#include "Model/Model.h"

#include <externals/nlohmann/json.hpp>

#include <algorithm>
#include <assert.h>
#include <fstream>
#include <numbers>

#include "externals/DirectXTex/d3dx12.h"

using namespace MathUtils;
using namespace MathGenerators;
using namespace BlendMode;

std::unique_ptr<ParticleManager> ParticleManager::instance_ = nullptr;

// シングルトン実装
ParticleManager* ParticleManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = std::make_unique<ParticleManager>(Token{});
	}
	return instance_.get();
}

void ParticleManager::Destroy() {
	instance_.reset();
}

ParticleManager::ParticleManager(Token) {
	// コンストラクタ
}

void ParticleManager::SetEmitter(const std::string& name,
	const ParticleEmitter& emitter) {
	// 1. 該当するパーティクルグループを検索
	auto it = particleGroups_.find(name);

	if (it == particleGroups_.end()) {
		// グループが見つからない場合はエラーまたはログ出力
		Logger::Log("Error: Particle group '" + name +
			"' not found when setting emitter.");
		return;
	}

	// 2. グループ内の emitter メンバに、渡された emitter インスタンスをコピー
	ParticleGroup& group = particleGroups_[name];
	group.emitter = emitter;

	// 3. (Optional) 頻度時刻をリセット（すぐに発生を開始させたい場合）
	group.emitter.SetFrequencyTime(0.0f);
}

std::vector<std::string> ParticleManager::GetParticleGroupNames() const {
	std::vector<std::string> names;
	for (const auto& pair : particleGroups_) {
		names.push_back(pair.first);
	}
	return names;
}

ParticleEmitter* ParticleManager::GetEmitter(const std::string& name) {
	auto it = particleGroups_.find(name);
	if (it != particleGroups_.end()) {
		return &it->second.emitter;
	}
	return nullptr;
}

const ParticleEmitter* ParticleManager::GetEmitter(const std::string& name) const {
	auto it = particleGroups_.find(name);
	if (it != particleGroups_.end()) {
		return &it->second.emitter;
	}
	return nullptr;
}

void ParticleManager::SetGroupTexture(const std::string& name, const std::string& textureFilePath) {
	auto it = particleGroups_.find(name);
	if (it != particleGroups_.end()) {
		it->second.materialData.textureFilePath = textureFilePath;
		TextureManager::GetInstance()->LoadTexture(textureFilePath);
		it->second.materialData.textureIndex = TextureManager::GetInstance()->GetSrvIndex(textureFilePath);
	}
}

std::string ParticleManager::GetGroupTexture(const std::string& name) const {
	auto it = particleGroups_.find(name);
	if (it != particleGroups_.end()) {
		return it->second.materialData.textureFilePath;
	}
	return "";
}

void ParticleManager::ReleaseIntermediateResources() {
	// GPUがコマンド実行を完了した後、保持していたアップロードリソースをクリアする
	intermediateResources_.clear();
}

// 終了
void ParticleManager::Finalize() {
	// リソースの解放
	// (ComPtrが解放されるため、明示的な解放は不要だが、mappedDataのUnmapは必要)
	for (auto& pair : particleGroups_) {
		ParticleGroup& group = pair.second;
		// MapしたリソースをUnmapする
		if (group.mappedData) {
			group.instanceResource->Unmap(0, nullptr);
			group.mappedData = nullptr;
		}
		/*if (group.materialMappedData) {
		  group.materialResource->Unmap(0, nullptr);
		  group.materialMappedData = nullptr;
		}*/
	}
	particleGroups_.clear(); // グループのクリア

	// 頂点リソースの解放
	vertexResource_.Reset();

	// シングルトンインスタンスの解放
	instance_.reset();
}

void ParticleManager::Initialize() {
	// BlendModeごとの設定は汎用関数から取得
	std::array<D3D12_BLEND_DESC, kCountOfBlendMode> blendDescs =
		CreateBlendStateDescs();

	// パイプラインマネージャを使って、ブレンドモードごとのPSOを生成
	for (size_t i = 0; i < kCountOfBlendMode; ++i) {
		particlePsoArray_[i] = PipelineManager::GetInstance()->CreateParticlePSO(blendDescs[i]);
	}

	// ランダムエンジンの初期化
	std::random_device seedGenerator;
	randomEngine_ = std::mt19937(seedGenerator());

	// デフォルト状態の設定
	isUpdate_ = true;
	useBillboard_ = true;

	// 頂点データの初期化 (四角形ポリゴン)
	// 単位四角形の頂点データ

	VertexData vertices[4] = {
		{{-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}}, // 左下
		{{-0.5f, 0.5f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},  // 左上
		{{0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},  // 右下
		{{0.5f, 0.5f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}}    // 右上
	};

	// インデックスデータで2つの三角形を構成
	const uint16_t indices[6] = { 0, 1, 2, 1, 3, 2 }; // 6つのインデックス
	const uint32_t kIndexSize = sizeof(indices);
	const UINT kVertexCount = _countof(vertices); // 4
	const UINT sizeVB = sizeof(VertexData) * kVertexCount;

	// 頂点リソースの生成

	// 1. Default Heap にリソースを生成 (テクスチャと同じ構造)
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeVB;
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	// ... (その他、バッファに必要な設定)
	vertexResourceDesc.SampleDesc.Count = 1;
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	const D3D12_HEAP_PROPERTIES defaultHeapProps =
		CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateCommittedResource(
		&defaultHeapProps, // Default Heap!
		D3D12_HEAP_FLAG_NONE, &vertexResourceDesc, D3D12_RESOURCE_STATE_COMMON,
		nullptr, IID_PPV_ARGS(&vertexResource_));
	assert(SUCCEEDED(hr));

	// 2. 中間アップロードリソースを生成
	ComPtr<ID3D12Resource> vertexUploadResource =
		DX12Context::GetInstance()->CreateBufferResource(
			sizeVB); // UploadHeapリソース生成はここで利用

	// 3. 中間リソースに頂点データを Map & Copy
	VertexData* mappedVertexData = nullptr;
	vertexUploadResource->Map(0, nullptr,
		reinterpret_cast<void**>(&mappedVertexData));
	memcpy(mappedVertexData, vertices, sizeVB);
	vertexUploadResource->Unmap(0, nullptr); // アンマップ

	// 4. コマンドリストにコピーを積む
	DX12Context::GetInstance()->GetCommandList()->CopyResource(
		vertexResource_.Get(), vertexUploadResource.Get());

	// 5. 状態遷移コマンドを積む
	D3D12_RESOURCE_BARRIER vertexBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		vertexResource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	DX12Context::GetInstance()->GetCommandList()->ResourceBarrier(1,
		&vertexBarrier);

	// 最後に、このリソースをリストに移動して保持する
	intermediateResources_.push_back(vertexUploadResource);

	// 頂点バッファビュー (VBV) 作成
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	vertexBufferView_.SizeInBytes = sizeVB;

	// リソースの生成

	// 1. Default Heap にリソースを生成 (テクスチャと同じ構造)
	D3D12_RESOURCE_DESC indexResourceDesc{};
	indexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	indexResourceDesc.Width = kIndexSize;
	indexResourceDesc.Height = 1;
	indexResourceDesc.DepthOrArraySize = 1;
	indexResourceDesc.MipLevels = 1;
	// ... (その他、バッファに必要な設定)
	indexResourceDesc.SampleDesc.Count = 1;
	indexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	const D3D12_HEAP_PROPERTIES defaultIndexHeapProps =
		CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	hr = DX12Context::GetInstance()->GetDevice()->CreateCommittedResource(
		&defaultIndexHeapProps, // Default Heap!
		D3D12_HEAP_FLAG_NONE, &indexResourceDesc, D3D12_RESOURCE_STATE_COMMON,
		nullptr, IID_PPV_ARGS(&indexResource_));
	assert(SUCCEEDED(hr));

	// 2. 中間アップロードリソースを生成
	ComPtr<ID3D12Resource> indexUploadResource =
		DX12Context::GetInstance()->CreateBufferResource(
			kIndexSize); // UploadHeapリソース生成はここで利用

	// 3. 中間リソースに頂点データを Map & Copy
	uint16_t* mappedIndexData = nullptr;
	HRESULT hrMap = indexUploadResource->Map(
		0, nullptr, reinterpret_cast<void**>(&mappedIndexData));
	assert(SUCCEEDED(hrMap));

	memcpy(mappedIndexData, indices, kIndexSize);
	indexUploadResource->Unmap(0, nullptr); // アンマップ

	// 4. コマンドリストにコピーを積む
	DX12Context::GetInstance()->GetCommandList()->CopyResource(
		indexResource_.Get(), indexUploadResource.Get());

	// 5. 状態遷移コマンドを積む (COPY_DEST → VERTEX_AND_CONSTANT_BUFFER)
	D3D12_RESOURCE_BARRIER indexBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		indexResource_.Get(), //
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	DX12Context::GetInstance()->GetCommandList()->ResourceBarrier(1,
		&indexBarrier);

	// 最後に、このリソースをリストに移動して保持する
	intermediateResources_.push_back(indexUploadResource);

	// --- IBV (Index Buffer View) の設定 ---
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = kIndexSize;
	indexBufferView_.Format =
		DXGI_FORMAT_R16_UINT;        // uint16_t を使用するため R16_UINT
	numIndices_ = _countof(indices); // 6を記録

	vertexBufferView_.SizeInBytes = sizeVB;
}

void ParticleManager::CreateParticleGroup(const std::string& name,
	const std::string& textureFilePath, Model* model) {
	// すでに登録済みのグループが存在する場合はリセットして再利用（シーン再遷移時の多重確保とアサートを防止）
	auto it = particleGroups_.find(name);
	if (it != particleGroups_.end()) {
		ParticleGroup& group = it->second;
		group.particlePool.Clear();
		group.instanceCount = 0;
		group.defaultModel = model;
		group.model = model;
		group.materialData.textureFilePath = textureFilePath;
		group.emitter.SetIsEmit(false);
		TextureManager::GetInstance()->LoadTexture(textureFilePath);
		return;
	}

	// 新たな空のパーティクルグループを作成し、コンテナに登録
	ParticleGroup newGroup;
	newGroup.particlePool.Initialize(kNumMaxParticle);
	newGroup.defaultModel = model;
	newGroup.model = model;
	newGroup.materialData.textureFilePath = textureFilePath;

	// マテリアルデータにテクスチャのSRVインデックスを記録
	newGroup.materialData.textureFilePath = textureFilePath;
	TextureManager::GetInstance()->LoadTexture(
		newGroup.materialData.textureFilePath);

	// 新しいパーティクルグループのインスタンシング用リソースの生成とSRVの確保/生成

	// インスタンシング用にSRVを確保してSRVインデックスとGPUハンドルを記録
	uint32_t instanceDataIndex = SrvManager::GetInstance()->Allocate();

	// インスタンシング用リソース（StructuredBuffer）を生成
	newGroup.instanceResource = DX12Context::GetInstance()->CreateBufferResource(
		sizeof(ParticleInstanceData) * kNumMaxParticle);

	// SRV生成 (StructuredBuffer用設定)
	// StructuredBuffer用のSRVを作成します
	SrvManager::GetInstance()->CreateSRVForStructuredBuffer(
		instanceDataIndex, newGroup.instanceResource, kNumMaxParticle,
		sizeof(ParticleInstanceData));

	// 描画時に使用するStructuredBufferのGPUハンドルをメンバに記録
	newGroup.instanceSrvHandleGPU =
		SrvManager::GetInstance()->GetGPUDescriptorHandle(instanceDataIndex);

	// 書き込み用ポインタを取得し、単位行列で初期化
	newGroup.instanceResource->Map(
		0, nullptr, reinterpret_cast<void**>(&newGroup.mappedData));

	// 初期値を書き込んでおく
	ParticleInstanceData* particleData =
		(ParticleInstanceData*)newGroup.mappedData;
	for (uint32_t index = 0; index < kNumMaxParticle; ++index) {
		particleData[index].WVP = MakeIdentity4x4();
		particleData[index].World = MakeIdentity4x4();
		particleData[index].color = { 0.0f, 0.0f, 0.0f, 0.0f };
		particleData[index].uvTransform = MakeIdentity4x4();
	}
	newGroup.instanceCount = 0;
	newGroup.emitter.SetIsEmit(false);

	// 1. マテリアルデータ用のCBVリソースを生成
	const UINT kCbvAlignedSize = 256;
	newGroup.materialResource = DX12Context::GetInstance()->CreateBufferResource(
		(sizeof(Material) + kCbvAlignedSize - 1) & ~(kCbvAlignedSize - 1));

	// 2. リソースをMapし、ポインタを取得
	HRESULT hr = newGroup.materialResource->Map(
		0, nullptr, reinterpret_cast<void**>(&newGroup.materialMappedData));
	assert(SUCCEEDED(hr));

	// 3. マテリアルにデータを書き込む
	newGroup.materialMappedData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	newGroup.materialMappedData->enableLighting =
		0; // パーティクルではライティングを無効化 (0)
	newGroup.materialMappedData->uvTransform = MakeIdentity4x4();

	// 追加パラメータの初期化
	newGroup.materialMappedData->innerColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	newGroup.materialMappedData->outerColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	newGroup.materialMappedData->fadeStartAlpha = 1.0f;
	newGroup.materialMappedData->fadeEndAlpha = 1.0f;
	newGroup.materialMappedData->fadeRange = 0.0f;
	newGroup.materialMappedData->isUvSwap = 0;
	newGroup.materialMappedData->isRing = 0;

	// グループをマップに追加
	particleGroups_.emplace(name, std::move(newGroup));
}

void ParticleManager::ClearParticles(const std::string& name) {
	auto it = particleGroups_.find(name);
	if (it != particleGroups_.end()) {
		it->second.particlePool.Clear();
		it->second.instanceCount = 0;
	}
}

void ParticleManager::ClearAllParticles() {
	for (auto& pair : particleGroups_) {
		pair.second.particlePool.Clear();
		pair.second.instanceCount = 0;
	}
}

void ParticleManager::Emit(const std::string& name, const Vector3& translate,
	uint32_t count) {
	// 該当するパーティクルグループを検索
	auto it = particleGroups_.find(name);
	if (it == particleGroups_.end()) {
		// グループが見つからない場合は処理を中止 (またはエラー処理)
		return;
	}

	ParticleGroup& group = it->second;

	// 指定された個数だけ Object Pool からスロットを取得して初期化
	for (uint32_t i = 0; i < count; ++i) {
		Particle* slot = group.particlePool.Acquire();
		if (!slot) {
			break; // プール上限到達
		}
		*slot = MakeNewParticle(translate, group.emitter.GetGenerateSettings());
	}
}

void ParticleManager::EmitCustom(const std::string& name, const Vector3& translate, const Vector3& rotate, const Vector3& scale, const Vector4& color, float lifeTime) {
	auto it = particleGroups_.find(name);
	if (it == particleGroups_.end()) {
		return;
	}

	ParticleGroup& group = it->second;

	Particle* slot = group.particlePool.Acquire();
	if (!slot) {
		return;
	}
	*slot = Particle{};
	slot->transform.translate = translate;
	slot->transform.rotate = rotate;
	slot->transform.scale = scale;
	slot->velocity = { 0.0f, 0.0f, 0.0f };
	slot->color = color;
	slot->lifeTime = lifeTime;
	slot->currentTime = 0.0f;
}

// パーティクル生成関数
Particle ParticleManager::MakeNewParticle(const Vector3& translate, const ParticleGenerateSettings& settings) {
	auto randRange = [this](float a, float b) {
		if (a > b) {
			std::swap(a, b);
		}
		return std::uniform_real_distribution<float>(a, b)(randomEngine_);
	};

	// パーティクルの初期化
	Particle particle;

	// -------------------
	// トランスフォーム
	// -------------------
	particle.transform.scale = settings.isRandomScale
		? Vector3{ randRange(settings.scaleMin.x, settings.scaleMax.x),
		           randRange(settings.scaleMin.y, settings.scaleMax.y),
		           randRange(settings.scaleMin.z, settings.scaleMax.z) }
		: settings.fixedScale;

	particle.transform.rotate = settings.isRandomRotate
		? Vector3{ randRange(settings.rotateMin.x, settings.rotateMax.x),
		           randRange(settings.rotateMin.y, settings.rotateMax.y),
		           randRange(settings.rotateMin.z, settings.rotateMax.z) }
		: settings.fixedRotate;

	particle.transform.translate = translate; // オフセットなしで基準位置に発生させる

	// -------------------
	// 速度
	// -------------------
	particle.velocity = settings.isRandomVelocity
		? Vector3{ randRange(settings.velocityMin.x, settings.velocityMax.x),
		           randRange(settings.velocityMin.y, settings.velocityMax.y),
		           randRange(settings.velocityMin.z, settings.velocityMax.z) }
		: settings.fixedVelocity;

	// -------------------
	// 寿命と時間
	// -------------------
	particle.lifeTime = settings.isRandomLifeTime
		? randRange(settings.lifeTimeMin, settings.lifeTimeMax)
		: settings.fixedLifeTime;
	particle.currentTime = 0.0f;

	// -------------------
	// 色
	// -------------------
	particle.color = settings.isRandomColor
		? Vector4{ randRange(settings.colorMin.x, settings.colorMax.x),
		           randRange(settings.colorMin.y, settings.colorMax.y),
		           randRange(settings.colorMin.z, settings.colorMax.z),
		           randRange(settings.colorMin.w, settings.colorMax.w) }
		: settings.fixedColor;

	return particle;
}

// ---------------------------------------------------------------------------
// Update ヘルパー: エミッターの時刻進行とパーティクル生成
// ---------------------------------------------------------------------------
void ParticleManager::UpdateGroupEmitter(ParticleGroup& group, const std::string& name, float deltaTime) {
	if (group.emitter.IsEffectMode()) {
		// エフェクトモードの場合（独立した別枠の「パーティクル生成システム」）
		bool hasActiveParticles = !group.particlePool.Empty();

		if (!hasActiveParticles) {
			// パーティクルがない（消滅した＝アニメーション完了）
			if (group.emitter.IsPlaying()) {
				// 再生中だったものが消滅した場合
				group.emitter.SetIsPlaying(false);

				if (group.emitter.IsLoop()) {
					// ループが有効なら、UV設定をリセットして最初から再再生
					group.emitter.ResetUVAnimationState();

					// 再度発生
					Emit(name, group.emitter.GetTranslate(), group.emitter.GetCount());
					group.emitter.SetIsPlaying(true);
				}
			} else {
				// 再生中でない（初期状態、または一度再生が終わってループ無効）
				// isEmit がオンになった瞬間に最初の1回を再生開始する
				if (group.emitter.IsEmit()) {
					group.emitter.ResetUVAnimationState();

					Emit(name, group.emitter.GetTranslate(), group.emitter.GetCount());
					group.emitter.SetIsPlaying(true);
				}
			}
		}
	} else {
		// 既存の frequency に基づく自動連続生成処理
		if (group.emitter.IsEmit()) {
			group.emitter.AddFrequencyTime(deltaTime);
			if (group.emitter.GetFrequency() > 0.0f &&
				group.emitter.GetFrequency() <= group.emitter.GetFrequencyTime()) {
				Emit(name, group.emitter.GetTranslate(), group.emitter.GetCount());
				group.emitter.SetFrequencyTime(group.emitter.GetFrequencyTime() - group.emitter.GetFrequency());
			}
		} else {
			// 自動発生が無効な場合は時刻をリセットし、再び有効になったときに一気に発生するのを防ぐ
			group.emitter.SetFrequencyTime(0.0f);
		}
	}
}

// ---------------------------------------------------------------------------
// Update ヘルパー: グローバル UV アニメーションとリングマテリアルの CPU → GPU 転送
// ---------------------------------------------------------------------------
void ParticleManager::UpdateGroupMaterial(ParticleGroup& group, float deltaTime) {
	if (!group.materialMappedData) return;

	// --- グローバル UV アニメーション ---
	const auto& uvas = group.emitter.GetUVAnimationSettings();
	if (uvas.isActive && !uvas.isIndividual) {
		group.emitter.AdvanceUVAnimation(deltaTime);
		const auto& updatedUvas = group.emitter.GetUVAnimationSettings();

		group.materialMappedData->uvTransform = MakeAffineMatrix(
			Vector3{ updatedUvas.currentScale.x, updatedUvas.currentScale.y, 1.0f },
			Vector3{ 0.0f, 0.0f, updatedUvas.currentRotate },
			Vector3{ updatedUvas.currentTranslate.x, updatedUvas.currentTranslate.y, 0.0f });
	} else {
		group.materialMappedData->uvTransform = MakeIdentity4x4();
	}

	// 形状に応じたマテリアル設定（isRing / isCylinder フラグ等）を委譲
	if (ParticleShape* shape = group.emitter.GetShape()) {
		shape->ApplyMaterial(group.materialMappedData);

		// 必要ならモデルを内部で再構築し、キャッシュされたモデルポインタを取得
		// (テクスチャパスが変わっていなければ再構築は走らず、前回作成したモデルが返る)
		Model* shapeModel = shape->GetOrBuildModel(group.materialData.textureFilePath);

		// シェイプ固有のモデルがあればそれを使い、なければデフォルトのモデル（Billboard等）を使う
		group.model = shapeModel ? shapeModel : group.defaultModel;
	}

	// --- 共通マテリアル処理（色やUVトランスフォーム以外の共通項目） ---
}

// ---------------------------------------------------------------------------
// Update ヘルパー: 単一パーティクルの物理・UV 更新
// 戻り値: true = まだ生存、false = 寿命切れ（呼び出し側が erase する）
// ---------------------------------------------------------------------------
bool ParticleManager::UpdateParticle(Particle& particle, const ParticleGroup& group, float deltaTime) {
	if (isUpdate_) {
		const auto& fs = group.emitter.GetFieldSettings();

		// 加速フィールド
		if (fs.isAccelerationFieldActive &&
			IsCollision(fs.accelerationField.area, particle.transform.translate)) {
			particle.velocity += fs.accelerationField.acceleration * deltaTime;
		}
		// 重力フィールド
		if (fs.isGravityFieldActive) {
			particle.velocity += fs.gravity * deltaTime;
		}
	}

	// 移動
	particle.transform.translate += particle.velocity * deltaTime;

	// 個別 UV アニメーション
	const auto& uvas = group.emitter.GetUVAnimationSettings();
	if (isUpdate_ && uvas.isActive && uvas.isIndividual) {
		particle.uvTranslate.x += uvas.scrollSpeed.x * deltaTime;
		particle.uvTranslate.y += uvas.scrollSpeed.y * deltaTime;
		particle.uvRotate      += uvas.rotateSpeed   * deltaTime;
		particle.uvScale.x     += uvas.scaleSpeed.x  * deltaTime;
		particle.uvScale.y     += uvas.scaleSpeed.y  * deltaTime;
	}

	// 時間経過
	particle.currentTime += deltaTime;

	// 寿命チェック
	return particle.currentTime < particle.lifeTime;
}

// ---------------------------------------------------------------------------
// Update ヘルパー: インスタンシングバッファへの書き込み
// ---------------------------------------------------------------------------
void ParticleManager::WriteInstanceData(
	ParticleGroup& group,
	const Camera& camera,
	const Matrix4x4& viewProjectionMatrix,
	uint32_t& instanceIndex)
{
	auto* instanceData = static_cast<ParticleInstanceData*>(group.mappedData);

	group.particlePool.ForEachActive([&](Particle& particle) {
		float alpha = 1.0f - (particle.currentTime / particle.lifeTime);

		if (instanceIndex < kNumMaxParticle) {
			// ワールド行列の計算
			Matrix4x4 scaleM  = MakeScaleMatrix(particle.transform.scale);
			Matrix4x4 rotateM = MakeRotateXYZMatrix(particle.transform.rotate);

			Matrix4x4 billboardM;
			if (group.emitter.GetShape() && group.emitter.GetShape()->NeedsBillboard() && useBillboard_) {
				billboardM = camera.GetWorldMatrix();
				billboardM.m[3][0] = billboardM.m[3][1] = billboardM.m[3][2] = 0.0f;
			} else {
				billboardM = MakeIdentity4x4();
			}

			Matrix4x4 worldM = Multiply(scaleM, Multiply(rotateM, billboardM));
			worldM.m[3][0] = particle.transform.translate.x;
			worldM.m[3][1] = particle.transform.translate.y;
			worldM.m[3][2] = particle.transform.translate.z;

			// UV 変換行列
			Matrix4x4 uvM;
			const auto& uvas = group.emitter.GetUVAnimationSettings();
			if (uvas.isActive) {
				if (uvas.isIndividual) {
					uvM = MakeAffineMatrix(
						Vector3{ particle.uvScale.x, particle.uvScale.y, 1.0f },
						Vector3{ 0.0f, 0.0f, particle.uvRotate },
						Vector3{ particle.uvTranslate.x, particle.uvTranslate.y, 0.0f });
				} else {
					uvM = group.materialMappedData->uvTransform;
				}
			} else {
				uvM = MakeIdentity4x4();
			}

			instanceData[instanceIndex].WVP         = Multiply(worldM, viewProjectionMatrix);
			instanceData[instanceIndex].World        = worldM;
			instanceData[instanceIndex].color        = particle.color;
			instanceData[instanceIndex].color.w      = alpha;
			instanceData[instanceIndex].uvTransform  = uvM;
			++instanceIndex;
		}
	});
}

// ---------------------------------------------------------------------------
// Update 本体 (スリム化済み)
// ---------------------------------------------------------------------------
void ParticleManager::Update(const Camera& camera, float deltaTime) {
	// ビュープロジェクション行列の算出
	const Matrix4x4 viewProjectionMatrix =
		Multiply(camera.GetViewMatrix(), camera.GetProjectionMatrix());

	for (auto& pair : particleGroups_) {
		ParticleGroup& group = pair.second;

		// 1. エミッター更新 (時刻管理／生成制御)
		UpdateGroupEmitter(group, pair.first, deltaTime);

		// 2. マテリアル・UV・リング更新
		UpdateGroupMaterial(group, deltaTime);

		// 3. パーティクル物理更新のみ行う（寿命切れはプールへ返却）
		group.particlePool.UpdateAndRelease([&](Particle& particle) {
			return UpdateParticle(particle, group, deltaTime);
		});

		// 4. インスタンスデータ書き込み
		uint32_t instanceIndex = 0;
		WriteInstanceData(group, camera, viewProjectionMatrix, instanceIndex);
		group.instanceCount = instanceIndex;
	}
}

void ParticleManager::Draw(BlendMode::BlendState blendMode) {
	// 1. コマンド: ルートシグネチャを設定
	DX12Context::GetInstance()->GetCommandList()->SetGraphicsRootSignature(
		PipelineManager::GetInstance()->GetParticleRootSignature());

	// 2. コマンド: PSO(Pipeline State Object)を設定
	DX12Context::GetInstance()->GetCommandList()->SetPipelineState(
		particlePsoArray_[blendMode].Get());
	// PSOのインデックスは、ParticleManager::Initializeで生成したブレンドモードの配列順に合わせる

	// 3. コマンド: プリミティブトポロジー (描画形状)を設定
	DX12Context::GetInstance()->GetCommandList()->IASetPrimitiveTopology(
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	// 5. 全てのパーティクルグループについて処理
	// 1グループ分で1DrawCallなので、こちらは二重for文にはならない。
	for (auto& pair : particleGroups_) {
		ParticleGroup& group = pair.second;

		// 描画インスタンス数が0の場合はスキップ
		if (group.instanceCount == 0) {
			continue;
		}

		// メッシュの設定
		if (group.model) {
			// モデル固有のメッシュを使用 (Ringなど)
			DX12Context::GetInstance()->GetCommandList()->IASetVertexBuffers(
				0, 1, &group.model->GetVertexBufferView());
			DX12Context::GetInstance()->GetCommandList()->IASetIndexBuffer(
				&group.model->GetIndexBufferView());
		}
		else {
			// デフォルトの矩形を使用
			DX12Context::GetInstance()->GetCommandList()->IASetVertexBuffers(
				0, 1, &vertexBufferView_);
			DX12Context::GetInstance()->GetCommandList()->IASetIndexBuffer(
				&indexBufferView_);
		}

		// コマンド：マテリアルデータのCBVを設定 (RootParameter[0])
		if (group.materialResource) {
			DX12Context::GetInstance()
				->GetCommandList()
				->SetGraphicsRootConstantBufferView(
					0, group.materialResource->GetGPUVirtualAddress());

		}
		else {
			// 警告またはアサート: マテリアルリソースがNULLです
			assert(false && "Material Resource is NULL in Draw");
		}

		// コマンド：インスタンシングデータのSRVのDescriptorTableを設定(RootParameter[1])
		DX12Context::GetInstance()
			->GetCommandList()
			->SetGraphicsRootDescriptorTable(1, group.instanceSrvHandleGPU);

		// コマンド：テクスチャのSRVのDescriptorTableを設定(RootParameter[2])
		DX12Context::GetInstance()
			->GetCommandList()
			->SetGraphicsRootDescriptorTable(
				2, TextureManager::GetInstance()->GetSrvHandleGPU(
					group.materialData.textureFilePath));

		// コマンド：DrawCall (インスタンシング描画)
		// インスタンス数: group.instanceCount
		if (group.instanceCount > 0) {
			uint32_t indexCount = group.model ? group.model->GetIndexCount() : numIndices_;
			DX12Context::GetInstance()->GetCommandList()->DrawIndexedInstanced(
				indexCount,
				group.instanceCount, 0, 0, 0);
		}

		// DX12Context::GetInstance()->GetCommandList()->DrawInstanced(6,
		// group.instanceCount, 0, 0);
	}
}

namespace {
Vector2 FromJsonVec2(const nlohmann::json& j, const Vector2& fallback = { 0.0f, 0.0f }) {
	if (j.is_array() && j.size() >= 2) {
		return { j[0].get<float>(), j[1].get<float>() };
	}
	return fallback;
}

Vector3 FromJsonVec3(const nlohmann::json& j, const Vector3& fallback = { 0.0f, 0.0f, 0.0f }) {
	if (j.is_array() && j.size() >= 3) {
		return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>() };
	}
	return fallback;
}

Vector4 FromJsonVec4(const nlohmann::json& j, const Vector4& fallback = { 1.0f, 1.0f, 1.0f, 1.0f }) {
	if (j.is_array() && j.size() >= 4) {
		return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>() };
	}
	return fallback;
}
} // namespace

bool ParticleManager::LoadEffectFromJson(
	const std::string& groupName,
	const std::string& filePath,
	BlendMode::BlendState* outBlendMode,
	bool* outUseBillboard) {
	using json = nlohmann::json;

	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		return false;
	}

	json root;
	try {
		ifs >> root;
	} catch (...) {
		return false;
	}

	std::string texPath = root.value("texture", "Particles/circle.png");
	TextureManager::GetInstance()->LoadTexture(texPath);

	// グループが未登録なら生成、既存ならテクスチャを更新
	if (GetEmitter(groupName) == nullptr) {
		CreateParticleGroup(groupName, texPath);
	} else {
		SetGroupTexture(groupName, texPath);
	}

	if (outBlendMode && root.contains("blendMode")) {
		int bm = root["blendMode"].get<int>();
		if (bm >= 0 && bm < BlendMode::BlendState::kCountOfBlendMode) {
			*outBlendMode = static_cast<BlendMode::BlendState>(bm);
		}
	}
	if (outUseBillboard && root.contains("useBillboard")) {
		*outUseBillboard = root["useBillboard"].get<bool>();
	}

	ParticleEmitter emitter;
	if (root.contains("emitter")) {
		const auto& je = root["emitter"];
		if (je.contains("transform")) {
			const auto& jt = je["transform"];
			Transform t;
			t.scale = FromJsonVec3(jt.value("scale", json::array()), { 1.0f, 1.0f, 1.0f });
			t.rotate = FromJsonVec3(jt.value("rotate", json::array()), { 0.0f, 0.0f, 0.0f });
			t.translate = FromJsonVec3(jt.value("translate", json::array()), { 0.0f, 0.0f, 0.0f });
			emitter.SetTransform(t);
		}
		emitter.SetCount(je.value("count", 1u));
		emitter.SetFrequency(je.value("frequency", 0.1f));
		emitter.SetIsEmit(je.value("isEmit", false));
		emitter.SetIsEffectMode(je.value("isEffectMode", false));
		emitter.SetIsLoop(je.value("isLoop", false));
	}

	if (root.contains("generateSettings")) {
		const auto& jg = root["generateSettings"];
		ParticleGenerateSettings gs = emitter.GetGenerateSettings();
		gs.isRandomScale = jg.value("isRandomScale", gs.isRandomScale);
		gs.scaleMin = FromJsonVec3(jg.value("scaleMin", json::array()), gs.scaleMin);
		gs.scaleMax = FromJsonVec3(jg.value("scaleMax", json::array()), gs.scaleMax);
		gs.fixedScale = FromJsonVec3(jg.value("fixedScale", json::array()), gs.fixedScale);

		gs.isRandomRotate = jg.value("isRandomRotate", gs.isRandomRotate);
		gs.rotateMin = FromJsonVec3(jg.value("rotateMin", json::array()), gs.rotateMin);
		gs.rotateMax = FromJsonVec3(jg.value("rotateMax", json::array()), gs.rotateMax);
		gs.fixedRotate = FromJsonVec3(jg.value("fixedRotate", json::array()), gs.fixedRotate);

		gs.isRandomVelocity = jg.value("isRandomVelocity", gs.isRandomVelocity);
		gs.velocityMin = FromJsonVec3(jg.value("velocityMin", json::array()), gs.velocityMin);
		gs.velocityMax = FromJsonVec3(jg.value("velocityMax", json::array()), gs.velocityMax);
		gs.fixedVelocity = FromJsonVec3(jg.value("fixedVelocity", json::array()), gs.fixedVelocity);

		gs.isRandomLifeTime = jg.value("isRandomLifeTime", gs.isRandomLifeTime);
		gs.lifeTimeMin = jg.value("lifeTimeMin", gs.lifeTimeMin);
		gs.lifeTimeMax = jg.value("lifeTimeMax", gs.lifeTimeMax);
		gs.fixedLifeTime = jg.value("fixedLifeTime", gs.fixedLifeTime);

		gs.isRandomColor = jg.value("isRandomColor", gs.isRandomColor);
		gs.colorMin = FromJsonVec4(jg.value("colorMin", json::array()), gs.colorMin);
		gs.colorMax = FromJsonVec4(jg.value("colorMax", json::array()), gs.colorMax);
		gs.fixedColor = FromJsonVec4(jg.value("fixedColor", json::array()), gs.fixedColor);
		emitter.SetGenerateSettings(gs);
	}

	if (root.contains("fieldSettings")) {
		const auto& jf = root["fieldSettings"];
		ParticleFieldSettings fsSettings = emitter.GetFieldSettings();
		fsSettings.isAccelerationFieldActive = jf.value("isAccelerationFieldActive", false);
		fsSettings.accelerationField.acceleration = FromJsonVec3(jf.value("acceleration", json::array()), fsSettings.accelerationField.acceleration);
		fsSettings.accelerationField.area.min = FromJsonVec3(jf.value("areaMin", json::array()), fsSettings.accelerationField.area.min);
		fsSettings.accelerationField.area.max = FromJsonVec3(jf.value("areaMax", json::array()), fsSettings.accelerationField.area.max);
		fsSettings.isGravityFieldActive = jf.value("isGravityFieldActive", false);
		fsSettings.gravity = FromJsonVec3(jf.value("gravity", json::array()), fsSettings.gravity);
		emitter.SetFieldSettings(fsSettings);
	}

	if (root.contains("uvAnimationSettings")) {
		const auto& ju = root["uvAnimationSettings"];
		ParticleUVAnimationSettings uvas = emitter.GetUVAnimationSettings();
		uvas.isActive = ju.value("isActive", false);
		uvas.isIndividual = ju.value("isIndividual", false);
		uvas.scrollSpeed = FromJsonVec2(ju.value("scrollSpeed", json::array()), uvas.scrollSpeed);
		uvas.rotateSpeed = ju.value("rotateSpeed", 0.0f);
		uvas.scaleSpeed = FromJsonVec2(ju.value("scaleSpeed", json::array()), uvas.scaleSpeed);
		emitter.SetUVAnimationSettings(uvas);
	}

	int shapeTypeInt = root.value("shapeType", 0);
	emitter.SetShapeType(static_cast<ParticleShapeType>(shapeTypeInt));

	if (auto* shape = emitter.GetShape()) {
		shape->LoadFromJson(root);
	}

	SetEmitter(groupName, emitter);
	return true;
}