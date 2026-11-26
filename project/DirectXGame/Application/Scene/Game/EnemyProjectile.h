#pragma once
#include "Object3d.h"
#include "Sprite.h"
#include <memory>

class EnemyProjectile {
public:
    enum class Type {
        Normal,
        Blast,   // 爆発弾（黄色）
        Jamming  // ジャミング弾（青色）
    };

    void Initialize(const Vector3& position, const Vector3& velocity, Type type = Type::Normal);
	// メインビュー用の更新
    void Update();
	// 指定したビュー用の更新
    void Update(uint32_t viewIndex, Camera* camera);
    void Draw(uint32_t viewIndex = 0);
    void UpdateHpGauge(Camera* camera, float clientWidth, float clientHeight);
    void DrawHpGauge();

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
    Type GetType() const { return type_; }
    bool IsExplosive() const { return type_ == Type::Blast; }
    bool IsJamming() const { return type_ == Type::Jamming; }

    bool IsHitShot() const { return isHitShot_; }
    void SetHitShot(bool hit) { isHitShot_ = hit; }

    float GetRadius() const { return radius_; }
    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    bool ApplyDamage(int damage = 1) {
        hp_ -= damage;
        if (hp_ <= 0) {
            hp_ = 0;
            isDead_ = true;
            isHpGaugeVisible_ = false;
            return true; // 破壊された
        }
        return false; // 耐久値残存
    }

private:
    std::unique_ptr<Object3d> object_;
    Vector3 position_;
    Vector3 velocity_;
    float radius_ = 0.5f;
    int hp_ = 1;
    int maxHp_ = 1;
    float damageBarRatio_ = 1.0f;
    bool isHpGaugeVisible_ = false;
    std::unique_ptr<Sprite> hpBgSprite_;
    std::unique_ptr<Sprite> hpDamageSprite_;
    std::unique_ptr<Sprite> hpFillSprite_;
    std::unique_ptr<Sprite> hpFrameSprite_;
    bool isDead_ = false;
    bool isHitShot_ = true;
    std::unique_ptr<Model> customModel_;
    Type type_ = Type::Normal;
    float trailRotation_ = 0.0f; // ライフリング回転角

    const std::string bulletModelDirectory_ = "Resources/Assets/Models/ShootingScene/bullet";
    const std::string bulletModelPath_ =
        "bullet.obj";
};
