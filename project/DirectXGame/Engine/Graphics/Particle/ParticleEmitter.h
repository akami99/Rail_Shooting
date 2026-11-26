#pragma once

#include "Types/GraphicsTypes.h"
#include "Types/ParticleTypes.h"
#include "ParticleShape.h"

#include <string>
#include <numbers>
#include <memory>

struct ParticleGenerateSettings {
	bool isRandomScale = true;
	Vector3 scaleMin = { 0.05f, 0.4f, 1.0f };
	Vector3 scaleMax = { 0.05f, 1.5f, 1.0f };
	Vector3 fixedScale = { 1.0f, 1.0f, 1.0f };

	bool isRandomRotate = true;
	Vector3 rotateMin = { 0.0f, 0.0f, -std::numbers::pi_v<float> };
	Vector3 rotateMax = { 0.0f, 0.0f, std::numbers::pi_v<float> };
	Vector3 fixedRotate = { 0.0f, 0.0f, 0.0f };

	bool isRandomVelocity = false;
	Vector3 velocityMin = { -1.0f, -1.0f, -1.0f };
	Vector3 velocityMax = { 1.0f, 1.0f, 1.0f };
	Vector3 fixedVelocity = { 0.0f, 0.0f, 0.0f };

	bool isRandomLifeTime = false;
	float lifeTimeMin = 1.0f;
	float lifeTimeMax = 3.0f;
	float fixedLifeTime = 1.0f;

	bool isRandomColor = true;
	Vector4 colorMin = { 0.0f, 0.0f, 0.0f, 1.0f };
	Vector4 colorMax = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 fixedColor = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct ParticleFieldSettings {
	bool isAccelerationFieldActive = false;
	AccelerationField accelerationField = { {15.0f, 0.0f, 0.0f}, {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}} };

	bool isGravityFieldActive = false;
	Vector3 gravity = { 0.0f, -9.8f, 0.0f };
};

struct ParticleUVAnimationSettings {
	bool isActive = false;
	bool isIndividual = false;            // 各パーティクル個別でアニメーションを行うか (エフェクト形式)
	Vector2 scrollSpeed = { 0.0f, 0.0f }; // U, V scroll speed per second
	float rotateSpeed = 0.0f;             // Rotation speed (rad/sec)
	Vector2 scaleSpeed = { 0.0f, 0.0f };  // Scale change speed per second

	// Current state values (for global animation)
	Vector2 currentTranslate = { 0.0f, 0.0f };
	float currentRotate = 0.0f;
	Vector2 currentScale = { 1.0f, 1.0f };
};

class ParticleEmitter {
public:
	// デフォルトコンストラクタ（shapeの初期化を追加）
	ParticleEmitter()
		: transform_{}, count_(1), frequency_(0.1f), frequencyTime_(0.0f), isEmit_(true),
		isEffectMode_(false), isLoop_(false), isPlaying_(false),
		shape_(std::make_unique<BillboardShape>()) {
	}

	// 引数付きコンストラクタ（shapeの初期化を追加）
	ParticleEmitter(const Transform& t, uint32_t c, float freq)
		: transform_(t), count_(c), frequency_(freq), frequencyTime_(0.0f), isEmit_(true),
		isEffectMode_(false), isLoop_(false), isPlaying_(false),
		shape_(std::make_unique<BillboardShape>()) {
	}

	// クローン（ディープコピー）のためのコピーコンストラクタとコピー代入演算子
	ParticleEmitter(const ParticleEmitter& other);
	ParticleEmitter& operator=(const ParticleEmitter& other);

	// ムーブコンストラクタとムーブ代入演算子も明示的にデフォルト定義
	ParticleEmitter(ParticleEmitter&& other) noexcept = default;
	ParticleEmitter& operator=(ParticleEmitter&& other) noexcept = default;

	~ParticleEmitter() = default;

	// 形状を取得 (型識別用)
	ParticleShapeType GetShapeType() const { return shape_ ? shape_->GetType() : ParticleShapeType::Billboard; }

	// 形状を切り替える (既存の Model リソースも解放される)
	void SetShapeType(ParticleShapeType type);

	// 形状ポインタの取得
	const ParticleShape* GetShape() const { return shape_.get(); }
	ParticleShape* GetShape() { return shape_.get(); }

	// 型を安全にキャストして取得するヘルパー
	const RingShape* GetRingShape() const { return dynamic_cast<const RingShape*>(shape_.get()); }
	RingShape* GetRingShape() { return dynamic_cast<RingShape*>(shape_.get()); }
	const CylinderShape* GetCylinderShape() const { return dynamic_cast<const CylinderShape*>(shape_.get()); }
	CylinderShape* GetCylinderShape() { return dynamic_cast<CylinderShape*>(shape_.get()); }
	const PlaneShape* GetPlaneShape() const { return dynamic_cast<const PlaneShape*>(shape_.get()); }
	PlaneShape* GetPlaneShape() { return dynamic_cast<PlaneShape*>(shape_.get()); }

	void Emit(const std::string& groupName);

	// --- ゲッター・セッター ---
	const Transform& GetTransform() const { return transform_; }
	void SetTransform(const Transform& transform) { transform_ = transform; }
	const Vector3& GetTranslate() const { return transform_.translate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	const Vector3& GetRotate() const { return transform_.rotate; }
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	const Vector3& GetScale() const { return transform_.scale; }
	void SetScale(const Vector3& scale) { transform_.scale = scale; }

	uint32_t GetCount() const { return count_; }
	void SetCount(uint32_t count) { count_ = count; }

	float GetFrequency() const { return frequency_; }
	void SetFrequency(float frequency) { frequency_ = frequency; }

	float GetFrequencyTime() const { return frequencyTime_; }
	void SetFrequencyTime(float frequencyTime) { frequencyTime_ = frequencyTime; }
	void AddFrequencyTime(float deltaTime) { frequencyTime_ += deltaTime; }

	bool IsEmit() const { return isEmit_; }
	void SetIsEmit(bool isEmit) { isEmit_ = isEmit; }

	bool IsEffectMode() const { return isEffectMode_; }
	void SetIsEffectMode(bool isEffectMode) { isEffectMode_ = isEffectMode; }

	bool IsLoop() const { return isLoop_; }
	void SetIsLoop(bool isLoop) { isLoop_ = isLoop; }

	bool IsPlaying() const { return isPlaying_; }
	void SetIsPlaying(bool isPlaying) { isPlaying_ = isPlaying; }

	const ParticleGenerateSettings& GetGenerateSettings() const { return generateSettings_; }
	void SetGenerateSettings(const ParticleGenerateSettings& settings) { generateSettings_ = settings; }

	const ParticleFieldSettings& GetFieldSettings() const { return fieldSettings_; }
	void SetFieldSettings(const ParticleFieldSettings& settings) { fieldSettings_ = settings; }

	const ParticleUVAnimationSettings& GetUVAnimationSettings() const { return uvAnimationSettings_; }
	void SetUVAnimationSettings(const ParticleUVAnimationSettings& settings) { uvAnimationSettings_ = settings; }
	void ResetUVAnimationState() {
		uvAnimationSettings_.currentTranslate = { 0.0f, 0.0f };
		uvAnimationSettings_.currentRotate = 0.0f;
		uvAnimationSettings_.currentScale = { 1.0f, 1.0f };
	}
	void AdvanceUVAnimation(float deltaTime) {
		uvAnimationSettings_.currentTranslate.x += uvAnimationSettings_.scrollSpeed.x * deltaTime;
		uvAnimationSettings_.currentTranslate.y += uvAnimationSettings_.scrollSpeed.y * deltaTime;
		uvAnimationSettings_.currentRotate += uvAnimationSettings_.rotateSpeed * deltaTime;
		uvAnimationSettings_.currentScale.x += uvAnimationSettings_.scaleSpeed.x * deltaTime;
		uvAnimationSettings_.currentScale.y += uvAnimationSettings_.scaleSpeed.y * deltaTime;
	}

private:
	// エミッターの設定値
	Transform transform_{}; // 位置、回転、スケール
	uint32_t count_;      // 1回で発生させる個数
	float frequency_;     // 発生頻度 (秒)
	float frequencyTime_; // 頻度計算用の時刻 (初期値 0.0f)
	bool isEmit_;         // 自動発生を有効にするかどうかのフラグ
	bool isEffectMode_ = false; // エフェクトモード（単発/ループ再生）にするか
	bool isLoop_ = false;       // アニメーション完了時にループ再生するか
	bool isPlaying_ = false;    // エフェクト再生中フラグ
	ParticleGenerateSettings generateSettings_; // 生成時の設定
	ParticleFieldSettings fieldSettings_;       // フィールドの設定
	ParticleUVAnimationSettings uvAnimationSettings_; // UVアニメーションの設定

	// 多態性を持った形状クラスのポインタ
	std::unique_ptr<ParticleShape> shape_;
};
