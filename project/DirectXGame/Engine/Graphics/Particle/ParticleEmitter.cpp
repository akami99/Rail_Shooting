#include "ParticleEmitter.h"
#include "ParticleManager.h"

// コピーコンストラクタ: shape の clone() を呼んで複製（ディープコピー）
ParticleEmitter::ParticleEmitter(const ParticleEmitter& other) {
	*this = other;
}

// コピー代入演算子
ParticleEmitter& ParticleEmitter::operator=(const ParticleEmitter& other) {
	if (this != &other) {
		transform_ = other.transform_;
		count_ = other.count_;
		frequency_ = other.frequency_;
		frequencyTime_ = other.frequencyTime_;
		isEmit_ = other.isEmit_;
		isEffectMode_ = other.isEffectMode_;
		isLoop_ = other.isLoop_;
		isPlaying_ = other.isPlaying_;
		generateSettings_ = other.generateSettings_;
		fieldSettings_ = other.fieldSettings_;
		uvAnimationSettings_ = other.uvAnimationSettings_;

		if (other.shape_) {
			shape_ = other.shape_->clone();
		} else {
			shape_ = std::make_unique<BillboardShape>();
		}
	}
	return *this;
}

// 形状の動的切り替え
void ParticleEmitter::SetShapeType(ParticleShapeType type) {
	// 現在の形状と異なる場合のみインスタンスを再生成
	if (GetShapeType() == type) return;

	switch (type) {
	case ParticleShapeType::Billboard:
		shape_ = std::make_unique<BillboardShape>();
		break;
	case ParticleShapeType::Ring:
		shape_ = std::make_unique<RingShape>();
		break;
	case ParticleShapeType::Cylinder:
		shape_ = std::make_unique<CylinderShape>();
		break;
	case ParticleShapeType::Plane:
		shape_ = std::make_unique<PlaneShape>();
		break;
	}
}

void ParticleEmitter::Emit(const std::string& groupName) {
	// エミッタの設定値に従って、ParticleManager::GetInstance()->Emit を呼び出す
	ParticleManager::GetInstance()->Emit(
		groupName,
		transform_.translate, // エミッタの現在位置
		count_                // 設定された個数
	);
}
