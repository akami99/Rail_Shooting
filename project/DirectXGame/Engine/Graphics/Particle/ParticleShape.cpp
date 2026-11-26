#include "ParticleShape.h"
#include "Model/Model.h"

// ============================================================
// BillboardShape の実装
// ============================================================
std::unique_ptr<ParticleShape> BillboardShape::clone() const {
    return std::make_unique<BillboardShape>(*this);
}

void BillboardShape::ApplyMaterial(Material* mat) const {
    if (!mat) return;
    mat->isRing = 0;
    mat->isCylinder = 0;

    mat->isUvSwap = 0;
    mat->fadeRange = 0.0f;
}

// ============================================================
// RingShape の実装
// ============================================================
RingShape::RingShape() = default;
RingShape::~RingShape() = default;

std::unique_ptr<ParticleShape> RingShape::clone() const {
    // model_ (unique_ptr) はコピーできないため、設定(settings_)のみを引き継いだ新しいインスタンスを生成
    auto copy = std::make_unique<RingShape>();
    copy->settings_ = this->settings_;
    // コピー直後はモデルが未作成なのでダーティフラグを立てる
    copy->MarkDirty();
    return copy;
}

void RingShape::ApplyMaterial(Material* mat) const {
    if (!mat) return;
    mat->isRing = 1;
    mat->isCylinder = 0;

    mat->isUvSwap = settings_.isUvSwap ? 1 : 0;
    mat->innerColor = settings_.innerColor;
    mat->outerColor = settings_.outerColor;
    mat->fadeStartAlpha = settings_.fadeStartAlpha;
    mat->fadeEndAlpha = settings_.fadeEndAlpha;
    mat->fadeRange = settings_.fadeRange;
}

Model* RingShape::GetOrBuildModel(const std::string& texPath) {
    // ダーティフラグが立っている、またはテクスチャパスが変わった場合のみ再構築
    if (NeedsRebuild(texPath)) {
        if (!model_) {
            model_ = std::make_unique<Model>();
        }

        // 既存のCreateRingの処理を呼び出す
        model_->CreateRing(texPath, settings_);

        MarkBuilt(texPath);
    }
    return model_.get();
}

// ============================================================
// CylinderShape の実装
// ============================================================
CylinderShape::CylinderShape() = default;
CylinderShape::~CylinderShape() = default;

std::unique_ptr<ParticleShape> CylinderShape::clone() const {
    auto copy = std::make_unique<CylinderShape>();
    copy->settings_ = this->settings_;
    copy->MarkDirty();
    return copy;
}

void CylinderShape::ApplyMaterial(Material* mat) const {
    if (!mat) return;
    mat->isRing = 0;
    mat->isCylinder = 1;
    mat->alphaReference = settings_.alphaReference;

    mat->isUvSwap = settings_.isUvSwap ? 1 : 0;
    mat->innerColor = settings_.topColor;      // topColor -> innerColor
    mat->outerColor = settings_.bottomColor;   // bottomColor -> outerColor
    mat->fadeStartAlpha = settings_.fadeStartAlpha;
    mat->fadeEndAlpha = settings_.fadeEndAlpha;
    mat->fadeRange = settings_.fadeRange;
}

Model* CylinderShape::GetOrBuildModel(const std::string& texPath) {
    if (NeedsRebuild(texPath)) {
        if (!model_) {
            model_ = std::make_unique<Model>();
        }

        // 既存のCreateCylinderの処理を呼び出す
        model_->CreateCylinder(texPath, settings_);

        MarkBuilt(texPath);
    }
    return model_.get();
}

// ============================================================
// PlaneShape の実装
// ============================================================
PlaneShape::PlaneShape() = default;
PlaneShape::~PlaneShape() = default;

std::unique_ptr<ParticleShape> PlaneShape::clone() const {
    auto copy = std::make_unique<PlaneShape>();
    copy->settings_ = this->settings_;
    copy->MarkDirty();
    return copy;
}

void PlaneShape::ApplyMaterial(Material* mat) const {
    if (!mat) return;
    mat->isRing = 0;
    mat->isCylinder = 0;

    mat->isUvSwap = settings_.isUvSwap ? 1 : 0;
    mat->innerColor = settings_.color;
    mat->outerColor = settings_.color;
    mat->fadeStartAlpha = 1.0f;
    mat->fadeEndAlpha = 1.0f;
    mat->fadeRange = 0.0f;
}

Model* PlaneShape::GetOrBuildModel(const std::string& texPath) {
    if (NeedsRebuild(texPath)) {
        if (!model_) {
            model_ = std::make_unique<Model>();
        }

        model_->CreatePlane(texPath, settings_);

        MarkBuilt(texPath);
    }
    return model_.get();
}