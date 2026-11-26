#pragma once
#include "Object3d.h"
#include "Sprite.h"
#include <memory>
#include <string>

/// <summary>
/// 敵弾の基底クラス（Normal / Blast / Jamming をポリモーフィックに扱う）
/// </summary>
class EnemyProjectile {
public:
    enum class Type {
        Normal,
        Blast,   // 爆発弾（黄色）
        Jamming  // ジャミング弾（青色）
    };

    virtual ~EnemyProjectile() = default;

    /// <summary>
    /// 弾種に応じた具象インスタンスを生成して初期化するファクトリメソッド
    /// </summary>
    static std::unique_ptr<EnemyProjectile> Create(Type type, const Vector3& position, const Vector3& velocity);

    void Initialize(const Vector3& position, const Vector3& velocity);
    // メインビュー用の更新
    void Update();
    // 指定したビュー用の更新
    void Update(uint32_t viewIndex, Camera* camera);
    void Draw(uint32_t viewIndex = 0);

    virtual void UpdateHpGauge(Camera* camera, float clientWidth, float clientHeight) {
        (void)camera;
        (void)clientWidth;
        (void)clientHeight;
    }
    virtual void DrawHpGauge() {}

    const Vector3& GetPosition() const { return position_; }
    void SetPosition(const Vector3& pos) {
        position_ = pos;
        if (object_) { object_->SetTranslate(pos); }
    }
    Vector3 GetRotation() const { return object_ ? object_->GetRotation() : Vector3{ 0.0f, 0.0f, 0.0f }; }
    void SetRotation(const Vector3& rot) { if (object_) { object_->SetRotation(rot); } }
    Vector3 GetScale() const { return object_ ? object_->GetScale() : Vector3{ 1.0f, 1.0f, 1.0f }; }
    void SetScale(const Vector3& scale) { if (object_) { object_->SetScale(scale); } }
    const Vector3& GetVelocity() const { return velocity_; }
    bool IsDead() const { return isDead_; }
    void Kill() { isDead_ = true; }

    virtual Type GetType() const = 0;
    virtual bool IsExplosive() const { return false; }
    virtual bool IsJamming() const { return false; }

    bool IsHitShot() const { return isHitShot_; }
    void SetHitShot(bool hit) { isHitShot_ = hit; }

    float GetRadius() const { return radius_; }
    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    virtual bool ApplyDamage(int damage = 1);

protected:
    virtual void SetupVisualAndStats() = 0;
    virtual void EmitTrailEffect(const Vector3& trailPos, const Vector3& rot) const = 0;

    std::unique_ptr<Object3d> object_;
    Vector3 position_{};
    Vector3 velocity_{};
    float radius_ = 0.5f;
    int hp_ = 1;
    int maxHp_ = 1;
    bool isDead_ = false;
    bool isHitShot_ = true;
    std::unique_ptr<Model> customModel_;
    float trailRotation_ = 0.0f; // ライフリング回転角

    const std::string bulletModelDirectory_ = "Resources/Assets/Models/ShootingScene/bullet";
    const std::string bulletModelPath_ = "bullet.obj";
};

/// <summary>
/// 通常弾
/// </summary>
class NormalEnemyProjectile : public EnemyProjectile {
public:
    Type GetType() const override { return Type::Normal; }

protected:
    void SetupVisualAndStats() override;
    void EmitTrailEffect(const Vector3& trailPos, const Vector3& rot) const override;
};

/// <summary>
/// 爆発弾（耐久値・頭上HPゲージ付き）
/// </summary>
class BlastEnemyProjectile : public EnemyProjectile {
public:
    Type GetType() const override { return Type::Blast; }
    bool IsExplosive() const override { return true; }

    void UpdateHpGauge(Camera* camera, float clientWidth, float clientHeight) override;
    void DrawHpGauge() override;
    bool ApplyDamage(int damage = 1) override;

protected:
    void SetupVisualAndStats() override;
    void EmitTrailEffect(const Vector3& trailPos, const Vector3& rot) const override;

private:
    float damageBarRatio_ = 1.0f;
    bool isHpGaugeVisible_ = false;
    std::unique_ptr<Sprite> hpBgSprite_;
    std::unique_ptr<Sprite> hpDamageSprite_;
    std::unique_ptr<Sprite> hpFillSprite_;
    std::unique_ptr<Sprite> hpFrameSprite_;
};

/// <summary>
/// ジャミング弾
/// </summary>
class JammingEnemyProjectile : public EnemyProjectile {
public:
    Type GetType() const override { return Type::Jamming; }
    bool IsJamming() const override { return true; }

protected:
    void SetupVisualAndStats() override;
    void EmitTrailEffect(const Vector3& trailPos, const Vector3& rot) const override;
};
