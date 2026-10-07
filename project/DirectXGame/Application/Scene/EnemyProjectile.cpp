#include "EnemyProjectile.h"
#include "ModelManager.h"
#include "Model.h"
#include "Particle/ParticleManager.h"
#include "MathUtils.h"
#include "MatrixGenerators.h"
#include <algorithm>
#include <cmath>

namespace {
    // 進行方向ベクトルとロール角(ライフリング)から、シリンダーのローカルY軸(円筒軸)を進行方向に向けたオイラー角(XYZ)を算出
    Vector3 CalculateTrailRotation(const Vector3& dir, float rollAngle) {
        Vector3 Y_axis = dir;
        Vector3 ref = (std::abs(dir.y) < 0.99f) ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);
        
        Vector3 X_base = MathUtils::Normalize(MathUtils::Cross(Y_axis, ref));
        Vector3 Z_base = MathUtils::Normalize(MathUtils::Cross(X_base, Y_axis));

        // 進行方向軸周りのライフリング回転を適用
        float cosR = std::cos(rollAngle);
        float sinR = std::sin(rollAngle);
        Vector3 X_final = MathUtils::Add(MathUtils::Multiply(cosR, X_base), MathUtils::Multiply(sinR, Z_base));
        Vector3 Z_final = MathUtils::Add(MathUtils::Multiply(-sinR, X_base), MathUtils::Multiply(cosR, Z_base));

        // 回転行列 R (行0: X, 行1: Y, 行2: Z)
        Matrix4x4 m = MathGenerators::MakeIdentity4x4();
        m.m[0][0] = X_final.x; m.m[0][1] = X_final.y; m.m[0][2] = X_final.z;
        m.m[1][0] = Y_axis.x;  m.m[1][1] = Y_axis.y;  m.m[1][2] = Y_axis.z;
        m.m[2][0] = Z_final.x; m.m[2][1] = Z_final.y; m.m[2][2] = Z_final.z;

        // MakeRotateXYZMatrix(rot) = Rx * (Ry * Rz) に対応するオイラー角の抽出
        Vector3 rot = { 0.0f, 0.0f, 0.0f };
        if (m.m[0][2] < 0.9999f) {
            if (m.m[0][2] > -0.9999f) {
                rot.y = std::asin(-std::clamp(m.m[0][2], -1.0f, 1.0f));
                rot.x = std::atan2(m.m[1][2], m.m[2][2]);
                rot.z = std::atan2(m.m[0][1], m.m[0][0]);
            } else {
                rot.y = 1.5707963f; // pi/2
                rot.x = -std::atan2(-m.m[1][0], m.m[1][1]);
                rot.z = 0.0f;
            }
        } else {
            rot.y = -1.5707963f; // -pi/2
            rot.x = std::atan2(-m.m[1][0], m.m[1][1]);
            rot.z = 0.0f;
        }
        return rot;
    }
}

void EnemyProjectile::Initialize(const Vector3& position, const Vector3& velocity, Type type) {
    position_ = position;
    velocity_ = velocity;
    type_ = type;
    trailRotation_ = 0.0f;
    
    object_ = std::make_unique<Object3d>();
    object_->Initialize();
    
    if (type_ == Type::Blast) {
        radius_ = 0.7f; // ロケット弾イメージで少し大きめ
        hp_ = 5;        // 5回で破壊可能
    } else {
        radius_ = 0.5f;
        hp_ = 1;
    }

    if (type_ != Type::Normal) {
        // 属性弾用に個別のモデルインスタンスを作成
        customModel_ = std::make_unique<Model>();
        customModel_->Initialize(bulletModelDirectory_, bulletModelPath_);
        
        if (type_ == Type::Blast) {
            customModel_->SetColor({1.0f, 0.8f, 0.0f, 1.0f}); // 黄色
        } else if (type_ == Type::Jamming) {
            customModel_->SetColor({0.0f, 0.5f, 1.0f, 1.0f}); // 青色
        }
        
        object_->SetModel(customModel_.get());
    } else {
        object_->SetModel(bulletModelPath_);
    }
    
    object_->SetTranslate(position_);
    object_->SetScale({radius_, radius_, radius_});
    float speed = MathUtils::Length(velocity_);
    if (speed > 0.0001f) {
        Vector3 dir = MathUtils::Normalize(velocity_);
        // 水平方向(Y軸回転)と上下方向(X軸回転)を算出
        float yaw = std::atan2(-dir.x, -dir.z);
        float xzLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
        float pitch = std::atan2(dir.y, xzLen);
        object_->SetRotation({ pitch, yaw, 0.0f });
    }
}

void EnemyProjectile::Update() {
    position_.x += velocity_.x;
    position_.y += velocity_.y;
    position_.z += velocity_.z;
    
    object_->SetTranslate(position_);
    // デフォルトのUpdate(全ビューをcamera_で更新)
    object_->Update();

    // 進行方向に対して垂直なライフリング半円トレイルを発生
    if (!isDead_) {
        float speed = MathUtils::Length(velocity_);
        if (speed > 0.0001f) {
            Vector3 dir = MathUtils::Normalize(velocity_);
            
            // ライフリング回転角を進める (毎フレーム約35度 = 0.6 rad ずつ回転)
            trailRotation_ += 0.6f;
            if (trailRotation_ > 6.2831853f) {
                trailRotation_ -= 6.2831853f;
            }

            // 進行方向に対して垂直なシリンダー半円の回転オイラー角を計算
            Vector3 rot = CalculateTrailRotation(dir, trailRotation_);

            // 弾の後方座標にトレイルを発生
            Vector3 trailPos = MathUtils::Subtract(position_, MathUtils::Multiply(radius_ * 0.4f, dir));

            std::string groupName = "BulletTrail_Normal";
            Vector4 color = { 1.0f, 1.0f, 1.0f, 0.85f };
            float lifeTime = 0.25f;
            Vector3 scale = { 1.0f, 1.0f, 1.0f };

            if (type_ == Type::Blast) {
                groupName = "BulletTrail_Blast";
                color = { 1.0f, 0.75f, 0.1f, 0.9f };
                lifeTime = 0.35f;
                scale = { 1.3f, 1.3f, 1.3f }; // 爆発弾は少し大きめ
            } else if (type_ == Type::Jamming) {
                groupName = "BulletTrail_Jamming";
                color = { 0.1f, 0.65f, 1.0f, 0.9f };
                lifeTime = 0.25f;
            }

            ParticleManager::GetInstance()->EmitCustom(groupName, trailPos, rot, scale, color, lifeTime);
        }
    }
}

void EnemyProjectile::Update(uint32_t viewIndex, Camera* camera) {
    // 位置などはUpdate()で更新済みなので、行列だけ再計算
    object_->Update(viewIndex, camera);
}

void EnemyProjectile::Draw(uint32_t viewIndex) {
    object_->Draw(viewIndex);
}
