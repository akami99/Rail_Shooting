#pragma once
#include "MathTypes.h"
#include "Sprite.h"
#include <memory>

class Camera;

/// <summary>
/// シューティングシーンのデモプレイAI（照準・射撃・カバー・武器切替・案内UI）を管理するクラス
/// </summary>
class ShootingDemoAI {
public:
    enum class BattleMode {
        Playing,
        SwarmBattle,
        GameOver,
        Other
    };

    struct Context {
        BattleMode mode = BattleMode::Other;
        const Camera* camera = nullptr;
        float gameTimer = 0.0f;
        bool isCovering = false;
        int playerHp = 0;
        int directDamage = 10;
        int ammo = 0;
        int maxAmmo = 9;
        int machineGunAmmo = 0;
        bool isMachineGun = false;
        float reloadDuration = 0.5f;
        float machineGunFireInterval = 0.1f;

        // Playing用ターゲット情報
        bool hasTargetEnemy = false;
        Vector3 targetEnemyPos{};
        bool hasTargetBlastProj = false;
        Vector3 targetBlastProjPos{};
        float minBlastProjDist = 99999.0f;
        bool hasIncomingNormalProj = false;

        // SwarmBattle用ターゲット情報
        int bossPhase = 1;
        bool hasSwarmTarget = false;
        Vector3 swarmTargetPos{};

        // GameOver用
        Vector2 retryPos{ 300.0f, 360.0f };
    };

    ShootingDemoAI() = default;
    ~ShootingDemoAI() = default;

    void Initialize();
    void Update(const Context& ctx);
    void DrawUI();

    const Vector2& GetMousePos() const { return mousePos_; }
    bool IsCovering() const { return isCovering_; }
    bool IsShootTriggered() const { return isShootTriggered_; }
    bool WantsSwitchToHandgun() const { return wantsSwitchToHandgun_; }
    bool WantsSwitchToMachineGun() const { return wantsSwitchToMachineGun_; }

private:
    Vector2 WorldToScreen(const Vector3& worldPos, const Camera* camera) const;
    void UpdateWeaponRotation(const Context& ctx, bool& isMachineGun);

private:
    std::unique_ptr<Sprite> pressSpaceSprite_;
    float blinkTimer_ = 0.0f;
    Vector2 mousePos_ = { 640.0f, 360.0f };
    bool isCovering_ = false;
    bool isShootTriggered_ = false;
    bool wantsSwitchToHandgun_ = false;
    bool wantsSwitchToMachineGun_ = false;
    float shootCooldown_ = 0.0f;
    float swarmBattleTimer_ = 0.0f;
    float minCoverTimer_ = 0.0f;
    float groundShootTimer_ = 0.0f;
    float groundShootCooldown_ = 3.0f;
    float machineGunTimer_ = 0.0f;
    float handgunTimer_ = 5.0f;
    bool needHandgunReload_ = false;
};
