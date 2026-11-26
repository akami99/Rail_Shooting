#pragma once
#include "MathTypes.h"
#include "Sprite.h"
#include <memory>
#include <string>
#include <vector>

class Camera;
class ShootingEffectManager;

/// <summary>
/// シューティングシーンのプレイヤー状態（HP・無敵・カバー）・武器・弾薬・照準・関連UIを管理するクラス
/// </summary>
class ShootingPlayerController {
public:
    enum class WeaponType {
        Handgun,    // ハンドガン (9発マガジン、単発)
        MachineGun  // マシンガン (60発マガジン、長押し連射)
    };

    static constexpr int kMaxHp = 30;
    static constexpr int kMaxAmmo = 9;
    static constexpr int kMaxMachineGunAmmo = 60;
    static constexpr int kEliteDropMachineGunAmmo = 20;
    static constexpr float kSectionJumpInvincibleDuration = 3.0f;
    static constexpr float kHandgunFireInterval = 0.2f;
    static constexpr float kMachineGunFireInterval = 0.1f;
    static constexpr float kReloadDuration = 0.5f;

    ShootingPlayerController() = default;
    ~ShootingPlayerController() = default;

    void Initialize(const std::string& crosshairPath);
    void Finalize();

    // 状態リセット
    void ResetState(bool resetMachineGunAmmo = true);
    void ResetAmmoUnitsLayout();
    void ResetCover();
    void SetForceCover(bool covering);

    // 毎フレーム更新
    void TickInvincibleTimer(float deltaTime);
    void UpdateCoverAndWeapon(bool isDemoMode, bool demoIsCovering, Camera* camera, ShootingEffectManager* effectManager);
    void UpdateCrosshair(const Vector2& mousePos, bool isAiming);
    void FadeCrosshair(float remainAlpha);
    bool TryFire(bool isDemoMode, bool demoShootTriggered);
    void UpdateUI(const Camera& camera, float clientWidth, float clientHeight);

    // イベント・ダメージ
    bool ApplyDamage(int damage);
    void HealToFull() { hp_ = kMaxHp; }
    void OnEliteEnemyDefeated(const Vector3& worldPos);

    // 描画
    void DrawAmmoAndWeaponUI();
    void DrawDropEffects();
    void DrawCrosshair();

    // 状態取得・設定
    int GetHp() const { return hp_; }
    bool IsCovering() const { return isCovering_; }
    bool IsForceInvincible(bool isAutoClearing) const {
        return isAutoClearing || isDebugInvincible_ || sectionJumpInvincibleTimer_ > 0.0f;
    }
    bool IsInvincible(bool isAutoClearing) const;
    WeaponType GetCurrentWeapon() const { return currentWeapon_; }
    void SetCurrentWeapon(WeaponType weapon) { currentWeapon_ = weapon; }
    int GetAmmo() const { return ammo_; }
    int GetMachineGunAmmo() const { return machineGunAmmo_; }
    bool HasCurrentWeaponAmmo() const;
    float GetEyeHeight() const { return eyeHeight_; }
    float GetCoverYOffset() const { return coverYOffset_; }
    void SetSectionJumpInvincible(float duration) { sectionJumpInvincibleTimer_ = duration; }

#ifdef USE_IMGUI
    void UpdateGlobalDebugImGui();
    void UpdateCoverImGui(bool isAutoClearing);
    void UpdateStatusImGui();
    void UpdateWeaponLayoutImGui();
#endif // USE_IMGUI

private:
    struct CrosshairSpriteSettings {
        Vector4 colorAim;
        Vector4 colorNoAim;
    };

    struct MachineGunAmmoDropEffect {
        Vector3 baseWorldPos;
        float timer = 0.0f;
        std::unique_ptr<Sprite> sprite;
        bool isVisible = false;
    };

    // プレイヤー・無敵・カバー状態
    int hp_ = kMaxHp;
    bool isCovering_ = false;
    float eyeHeight_ = 1.4f;
    float coverDepth_ = -0.7f;
    float coverEasingSpeed_ = 0.15f;
    float coverYOffset_ = 0.0f;
    float coverYTarget_ = -0.55f;
    bool isDebugInfiniteAmmo_ = false;
    bool isDebugInvincible_ = false;
    float sectionJumpInvincibleTimer_ = 0.0f;

    // 武器・弾薬状態
    WeaponType currentWeapon_ = WeaponType::Handgun;
    int ammo_ = kMaxAmmo;
    int machineGunAmmo_ = 0;
    float shootCooldownTimer_ = 0.0f;
    float reloadTimer_ = 0.0f;
    int lastReloadCount_ = 0;
    float ammoGetEffectTimer_ = 0.0f;

    // 照準スプライト
    std::unique_ptr<Sprite> crosshair_;
    CrosshairSpriteSettings crosshairSettings_ = {
        { 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f, 0.4f }
    };

    // 弾薬UI
    std::unique_ptr<Sprite> ammoBg_;
    std::vector<std::unique_ptr<Sprite>> ammoUnits_;
    std::vector<float> ammoCurrentX_;
    std::vector<float> ammoTargetX_;
    std::unique_ptr<Sprite> machineGunAmmoBar_;

    // 武器選択UI
    std::unique_ptr<Sprite> weaponBg_;
    std::unique_ptr<Sprite> weaponSelectCursor_;
    std::unique_ptr<Sprite> weaponHandgunIcon_;
    std::unique_ptr<Sprite> weaponMachineGunIcon_;
    std::unique_ptr<Sprite> weaponMachineGunAmmoGetIcon_;
    Vector2 weaponBgPos_ = { 640.0f, 665.0f };
    Vector2 weaponBgSize_ = { 220.0f, 90.0f };
    Vector2 weaponHandgunPos_ = { 585.0f, 665.0f };
    Vector2 weaponMachineGunPos_ = { 695.0f, 665.0f };
    Vector2 weaponIconSize_ = { 80.0f, 40.0f };
    Vector2 weaponCursorSize_ = { 76.0f, 76.0f };

    // 弾丸ドロップ演出
    std::vector<MachineGunAmmoDropEffect> ammoDropEffects_;
};
