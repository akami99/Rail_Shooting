#pragma once
#include "Object3d.h"
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

    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetVelocity() const { return velocity_; }
    bool IsDead() const { return isDead_; }
    void Kill() { isDead_ = true; }
    Type GetType() const { return type_; }
    bool IsExplosive() const { return type_ == Type::Blast; }
    bool IsJamming() const { return type_ == Type::Jamming; }

    float GetRadius() const { return radius_; }
    int GetHp() const { return hp_; }
    bool ApplyDamage(int damage = 1) {
        hp_ -= damage;
        if (hp_ <= 0) {
            hp_ = 0;
            isDead_ = true;
            return true; // 破壊された
        }
        return false; // 耐久値残存
    }

#ifdef USE_IMGUI
    // ImGui用のゲッター
    Object3d &GetObjectDebug() const { return *object_; }
#endif // USE_IMGUI

private:
    std::unique_ptr<Object3d> object_;
    Vector3 position_;
    Vector3 velocity_;
    float radius_ = 0.5f;
    int hp_ = 1;
    bool isDead_ = false;
    std::unique_ptr<Model> customModel_;
    Type type_ = Type::Normal;
    float trailRotation_ = 0.0f; // ライフリング回転角

    const std::string bulletModelDirectory_ = "Resources/Assets/Models/ShootingScene/bullet";
    const std::string bulletModelPath_ =
        "bullet.obj";
};
