#pragma once
#include "Types/GraphicsTypes.h"
#include "Types/ParticleTypes.h"
#include <memory>
#include <string>

class Model;

// 形状の種類を識別するための列挙型 (ImGui コンボボックス用)
enum class ParticleShapeType {
    Billboard = 0,
    Ring = 1,
    Cylinder = 2,
    Plane = 3,
};

// ============================================================
// ParticleShape — 抽象基底クラス
// ============================================================
class ParticleShape {
public:
    virtual ~ParticleShape() = default;

    // ディープコピー用の仮想関数
    virtual std::unique_ptr<ParticleShape> clone() const = 0;

    // マテリアル定数バッファへの設定転送 (純粋仮想)
    virtual void ApplyMaterial(Material* mat) const = 0;

    // 使用するモデルを返す。nullptr = デフォルトの矩形(Billboard)を使用
    // 設定が変更された場合にのみ内部で再構築する（ダーティフラグ）
    virtual Model* GetOrBuildModel(const std::string& texPath) = 0;

    // このシェイプがビルボード変換を必要とするか
    virtual bool NeedsBillboard() const { return false; }

    // ImGui の型識別用
    virtual ParticleShapeType GetType() const = 0;

    // 設定変更があった場合に呼ぶ (ダーティフラグをセット)
    void MarkDirty() { isDirty_ = true; }

protected:
    bool NeedsRebuild(const std::string& texPath) const { return isDirty_ || lastTexPath_ != texPath; }
    void MarkBuilt(const std::string& texPath) {
        isDirty_ = false;
        lastTexPath_ = texPath;
    }

private:
    bool isDirty_ = true;
    std::string lastTexPath_;
};

// ============================================================
// BillboardShape — ビルボードPlane / ヒットエフェクト (Plane回転)
// ============================================================
class BillboardShape : public ParticleShape {
public:
    std::unique_ptr<ParticleShape> clone() const override;
    void ApplyMaterial(Material* mat) const override;
    Model* GetOrBuildModel(const std::string& texPath) override { return nullptr; }
    bool NeedsBillboard() const override { return true; }
    ParticleShapeType GetType() const override { return ParticleShapeType::Billboard; }
};

// ============================================================
// RingShape — リングプリミティブ
// ============================================================
class RingShape : public ParticleShape {
public:
    RingShape();
    ~RingShape() override;

    const RingSettings& GetSettings() const { return settings_; }
    void SetSettings(const RingSettings& newSettings) {
        settings_ = newSettings;
        MarkDirty();
    }

    std::unique_ptr<ParticleShape> clone() const override;
    void ApplyMaterial(Material* mat) const override;
    Model* GetOrBuildModel(const std::string& texPath) override;
    ParticleShapeType GetType() const override { return ParticleShapeType::Ring; }

private:
    RingSettings settings_;
    std::unique_ptr<Model> model_;
};

// ============================================================
// CylinderShape — シリンダープリミティブ
// ============================================================
class CylinderShape : public ParticleShape {
public:
    CylinderShape();
    ~CylinderShape() override;

    const CylinderSettings& GetSettings() const { return settings_; }
    void SetSettings(const CylinderSettings& newSettings) {
        settings_ = newSettings;
        MarkDirty();
    }

    std::unique_ptr<ParticleShape> clone() const override;
    void ApplyMaterial(Material* mat) const override;
    Model* GetOrBuildModel(const std::string& texPath) override;
    ParticleShapeType GetType() const override { return ParticleShapeType::Cylinder; }

private:
    CylinderSettings settings_;
    std::unique_ptr<Model> model_;
};

// ============================================================
// PlaneShape — 平面プリミティブ
// ============================================================
class PlaneShape : public ParticleShape {
public:
    PlaneShape();
    ~PlaneShape() override;

    const PlaneSettings& GetSettings() const { return settings_; }
    void SetSettings(const PlaneSettings& newSettings) {
        settings_ = newSettings;
        MarkDirty();
    }

    std::unique_ptr<ParticleShape> clone() const override;
    void ApplyMaterial(Material* mat) const override;
    Model* GetOrBuildModel(const std::string& texPath) override;
    ParticleShapeType GetType() const override { return ParticleShapeType::Plane; }

private:
    PlaneSettings settings_;
    std::unique_ptr<Model> model_;
};