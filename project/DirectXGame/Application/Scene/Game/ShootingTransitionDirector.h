#pragma once
#include "MathTypes.h"
#include "Object3d.h"
#include <cstdint>
#include <memory>

class Camera;
class ShootingHUD;
class ShootingPlayerController;
class ShootingWaveManager;
class Skybox;
class StageRailManager;

/// <summary>
/// シューティングシーンの入場・クリア・タイトル戻り・リスタートスムージングの遷移演出を管理するクラス
/// </summary>
class ShootingTransitionDirector {
public:
    struct IntroContext {
        float& phaseTimer;
        bool isPaused;
        StageRailManager* railManager;
        ShootingPlayerController* playerController;
        ShootingWaveManager* waveManager;
        ShootingHUD* hud;
        Camera* camera;
        Skybox* skybox;
        float& cameraBasePitch;
        float& cameraYaw;
        float cameraBaseRoll;
        float baseCombatPitch;
        float baseCombatYaw;
        int mainViewIndex;
    };

    static constexpr float kIntroConstructDuration = 2.5f;
    static constexpr float kBlinkDuration = 1.0f;
    static constexpr float kFastForwardDuration = 2.0f;
    static constexpr float kSwarmShowcaseDuration = 2.5f;
    static constexpr float kRewindDuration = 1.5f;
    static constexpr float kClearTransitionDuration = 2.6f;
    static constexpr float kReturnToTitleDuration = 0.8f;
    static constexpr float kSmoothingOutDuration = 2.0f;

    ShootingTransitionDirector() = default;
    ~ShootingTransitionDirector() = default;

    void InitializeBlackout();

    // 遷移開始設定
    void BeginClearVignette();
    void BeginReturnToTitleVignette();
    void BeginRestartSmoothing();

    // 入場カットシーン State Pattern 制御
    void StartIntroConstruct();
    void StartIntroFastForward();
    void StartIntroReadyGo();
    bool UpdateIntro(IntroContext& ctx);
    bool NeedsCrosshairUpdateInIntro() const;
    bool IsIntroConstructActive() const;
    bool IsIntroReadyGoActive() const;

    // 各フェーズ更新（終了条件到達時に true を返す）
    bool UpdateIntroConstruct(float& phaseTimer, StageRailManager* railManager, Camera* camera, Skybox* skybox, int mainViewIndex);
    bool UpdatePreviewMode(StageRailManager* railManager, ShootingPlayerController* playerController,
                           ShootingWaveManager* waveManager, Camera* camera, Skybox* skybox,
                           float cameraBasePitch, float cameraYaw, int mainViewIndex);
    bool UpdateIntroFastForward(float& phaseTimer, StageRailManager* railManager, ShootingPlayerController* playerController,
                                ShootingWaveManager* waveManager, Camera* camera, Skybox* skybox, int mainViewIndex);
    bool UpdateIntroSwarmShowcase(float& phaseTimer, StageRailManager* railManager, ShootingPlayerController* playerController,
                                  ShootingWaveManager* waveManager, Camera* camera, Skybox* skybox,
                                  float cameraBasePitch, float cameraYaw, int mainViewIndex);
    bool UpdateIntroRewind(float& phaseTimer, StageRailManager* railManager, ShootingPlayerController* playerController,
                           ShootingWaveManager* waveManager, Camera* camera, Skybox* skybox,
                           float& cameraBasePitch, float& cameraYaw, float cameraBaseRoll,
                           float baseCombatPitch, float baseCombatYaw, int mainViewIndex);
    bool UpdateIntroReadyGo(float& phaseTimer, ShootingHUD* hud, StageRailManager* railManager,
                            Camera* camera, Skybox* skybox, int mainViewIndex);
    bool UpdateReturnToTitleVignette(float& phaseTimer, StageRailManager* railManager,
                                     Camera* camera, Skybox* skybox, int mainViewIndex);
    bool UpdateClearVignette(float& phaseTimer, StageRailManager* railManager,
                             Camera* camera, Skybox* skybox, int mainViewIndex);
    bool UpdateRestartSmoothing(float& phaseTimer);

    // ワイヤーフレーム＆ディゾルブ遷移描画
    void DrawObjectWithIntroTransition(Object3d* obj, float startTime, float duration,
                                       bool isIntroConstruct, float phaseTimer, int blendMode) const;
    void DrawObjectWithClearTransition(Object3d* obj, float startTime, float duration,
                                       bool isClearVignette, float phaseTimer, int blendMode) const;

    // 取得・設定
    const Vector3& GetTransitionEdgeColor() const { return kTransitionEdgeColor; }
    const Vector3& GetIntroSwarmOffset() const { return introSwarmOffset_; }
    float GetIntroSwarmRadius() const { return introSwarmRadius_; }
    uint32_t GetIntroSwarmCount() const { return introSwarmCount_; }
    void ClearPreviewMode() { isIntroPreviewMode_ = false; }

#ifdef USE_IMGUI
    struct IntroImGuiActions {
        bool replayIntro = false;
        bool playReadyGoOnly = false;
    };
    IntroImGuiActions UpdateIntroImGui(StageRailManager* railManager, ShootingWaveManager* waveManager, ShootingHUD* hud);
#endif // USE_IMGUI

private:
    class IIntroState {
    public:
        virtual ~IIntroState() = default;
        virtual void Update(ShootingTransitionDirector& director, IntroContext& ctx) = 0;
        virtual bool NeedsCrosshairUpdate() const { return false; }
        virtual bool IsConstructState() const { return false; }
        virtual bool IsReadyGoState() const { return false; }
    };
    class IntroConstructState;
    class IntroFastForwardState;
    class IntroSwarmShowcaseState;
    class IntroRewindState;
    class IntroReadyGoState;

    void ChangeIntroState(std::unique_ptr<IIntroState> nextState);
    std::unique_ptr<IIntroState> introState_;

    static inline const Vector3 kTransitionEdgeColor = { 0.0f, 1.0f, 0.8f };
    static inline const Vector4 kWireframeColor = { 0.0f, 0.9f, 1.0f, 1.0f };

    // スタート演出・終点スウォーム調整用パラメータ
    Vector3 introSwarmOffset_ = { 0.0f, 7.5f, 20.0f };
    float introSwarmRadius_ = 60.0f;
    uint32_t introSwarmCount_ = 100;
    float introCameraStopOffset_ = 0.0f;
    float introCameraPitch_ = -0.25f;
    float introCameraYaw_ = 0.0f;
    bool isIntroPreviewMode_ = false;

    // ビネット・スムージング演出パラメータ
    float vignetteScale_ = 16.0f;
    float vignetteExponent_ = 0.8f;
    float smoothingKernel_ = 31.0f;
};
