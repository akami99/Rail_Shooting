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

namespace {
nlohmann::json ShapeVecToJson(const Vector2& v) { return nlohmann::json::array({ v.x, v.y }); }
nlohmann::json ShapeVecToJson(const Vector4& v) { return nlohmann::json::array({ v.x, v.y, v.z, v.w }); }

Vector2 ShapeVec2FromJson(const nlohmann::json& j, const Vector2& def) {
    if (j.is_array() && j.size() >= 2) {
        return { j[0].get<float>(), j[1].get<float>() };
    }
    return def;
}

Vector4 ShapeVec4FromJson(const nlohmann::json& j, const Vector4& def) {
    if (j.is_array() && j.size() >= 4) {
        return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>() };
    }
    return def;
}
} // namespace

void RingShape::SaveToJson(nlohmann::json& root) const {
    root["ringSettings"] = {
        { "innerRadius", settings_.innerRadius },
        { "startOuterRadius", settings_.startOuterRadius },
        { "midOuterRadius", settings_.midOuterRadius },
        { "endOuterRadius", settings_.endOuterRadius },
        { "startAngle", settings_.startAngle },
        { "endAngle", settings_.endAngle },
        { "division", settings_.division },
        { "isUvSwap", settings_.isUvSwap },
        { "innerColor", ShapeVecToJson(settings_.innerColor) },
        { "outerColor", ShapeVecToJson(settings_.outerColor) },
        { "fadeStartAlpha", settings_.fadeStartAlpha },
        { "fadeEndAlpha", settings_.fadeEndAlpha },
        { "fadeRange", settings_.fadeRange }
    };
}

void RingShape::LoadFromJson(const nlohmann::json& root) {
    if (!root.contains("ringSettings")) {
        return;
    }
    const auto& jr = root["ringSettings"];
    RingSettings s = settings_;
    s.innerRadius = jr.value("innerRadius", s.innerRadius);
    s.startOuterRadius = jr.value("startOuterRadius", s.startOuterRadius);
    s.midOuterRadius = jr.value("midOuterRadius", s.midOuterRadius);
    s.endOuterRadius = jr.value("endOuterRadius", s.endOuterRadius);
    s.startAngle = jr.value("startAngle", s.startAngle);
    s.endAngle = jr.value("endAngle", s.endAngle);
    s.division = jr.value("division", s.division);
    s.isUvSwap = jr.value("isUvSwap", s.isUvSwap);
    s.innerColor = ShapeVec4FromJson(jr.value("innerColor", nlohmann::json::array()), s.innerColor);
    s.outerColor = ShapeVec4FromJson(jr.value("outerColor", nlohmann::json::array()), s.outerColor);
    s.fadeStartAlpha = jr.value("fadeStartAlpha", s.fadeStartAlpha);
    s.fadeEndAlpha = jr.value("fadeEndAlpha", s.fadeEndAlpha);
    s.fadeRange = jr.value("fadeRange", s.fadeRange);
    SetSettings(s);
}

void CylinderShape::SaveToJson(nlohmann::json& root) const {
    root["cylinderSettings"] = {
        { "height", settings_.height },
        { "topRadius", ShapeVecToJson(settings_.topRadius) },
        { "bottomRadius", ShapeVecToJson(settings_.bottomRadius) },
        { "startAngle", settings_.startAngle },
        { "endAngle", settings_.endAngle },
        { "division", settings_.division },
        { "verticalDivision", settings_.verticalDivision },
        { "flipV", settings_.flipV },
        { "isUvSwap", settings_.isUvSwap },
        { "topColor", ShapeVecToJson(settings_.topColor) },
        { "bottomColor", ShapeVecToJson(settings_.bottomColor) },
        { "fadeStartAlpha", settings_.fadeStartAlpha },
        { "fadeEndAlpha", settings_.fadeEndAlpha },
        { "fadeRange", settings_.fadeRange },
        { "alphaReference", settings_.alphaReference }
    };
}

void CylinderShape::LoadFromJson(const nlohmann::json& root) {
    if (!root.contains("cylinderSettings")) {
        return;
    }
    const auto& jc = root["cylinderSettings"];
    CylinderSettings s = settings_;
    s.height = jc.value("height", s.height);
    s.topRadius = ShapeVec2FromJson(jc.value("topRadius", nlohmann::json::array()), s.topRadius);
    s.bottomRadius = ShapeVec2FromJson(jc.value("bottomRadius", nlohmann::json::array()), s.bottomRadius);
    s.startAngle = jc.value("startAngle", s.startAngle);
    s.endAngle = jc.value("endAngle", s.endAngle);
    s.division = jc.value("division", s.division);
    s.verticalDivision = jc.value("verticalDivision", s.verticalDivision);
    s.flipV = jc.value("flipV", s.flipV);
    s.isUvSwap = jc.value("isUvSwap", s.isUvSwap);
    s.topColor = ShapeVec4FromJson(jc.value("topColor", nlohmann::json::array()), s.topColor);
    s.bottomColor = ShapeVec4FromJson(jc.value("bottomColor", nlohmann::json::array()), s.bottomColor);
    s.fadeStartAlpha = jc.value("fadeStartAlpha", s.fadeStartAlpha);
    s.fadeEndAlpha = jc.value("fadeEndAlpha", s.fadeEndAlpha);
    s.fadeRange = jc.value("fadeRange", s.fadeRange);
    s.alphaReference = jc.value("alphaReference", s.alphaReference);
    SetSettings(s);
}

void PlaneShape::SaveToJson(nlohmann::json& root) const {
    root["planeSettings"] = {
        { "size", ShapeVecToJson(settings_.size) },
        { "divisionX", settings_.divisionX },
        { "divisionY", settings_.divisionY },
        { "flipV", settings_.flipV },
        { "isUvSwap", settings_.isUvSwap },
        { "color", ShapeVecToJson(settings_.color) }
    };
}

void PlaneShape::LoadFromJson(const nlohmann::json& root) {
    if (!root.contains("planeSettings")) {
        return;
    }
    const auto& jp = root["planeSettings"];
    PlaneSettings s = settings_;
    s.size = ShapeVec2FromJson(jp.value("size", nlohmann::json::array()), s.size);
    s.divisionX = jp.value("divisionX", s.divisionX);
    s.divisionY = jp.value("divisionY", s.divisionY);
    s.flipV = jp.value("flipV", s.flipV);
    s.isUvSwap = jp.value("isUvSwap", s.isUvSwap);
    s.color = ShapeVec4FromJson(jp.value("color", nlohmann::json::array()), s.color);
    SetSettings(s);
}