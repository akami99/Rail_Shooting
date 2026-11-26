#pragma once
#include "BaseScene.h"
#include <vector>
#include <memory>
#include "MathTypes.h"

// 必要なクラスをインクルード or 前方宣言
#include "DebugCamera.h"
#include "Camera.h"
#include "Object3d.h"
#include "Sprite.h"
#include "Skybox.h"
#include "BlendMode.h"
#include "LevelLoader.h"

class DemoScene : public BaseScene {
public:
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    // 定数
    static inline const float kMoveSpeed = 0.1f;
    static inline const float kRotateSpeed = 0.02f;
    static inline const Vector3 kDefaultCameraRotate = { 0.3f, 0.0f, 0.0f };
    static inline const Vector3 kDefaultCameraTranslate = { 0.0f, 4.0f, -10.0f };

    // ゲームカメラの更新
    void UpdateGameCamera();

    // レベルデータからオブジェクトを再帰的に生成
    void CreateObjects(const std::vector<LevelData::ObjectData>& data);

    // プレイヤースポーンポイントの読み込み用ダミー関数
    void CreatePlayerSpawns(const std::vector<LevelData::PlayerSpawnData>& data);

#ifdef USE_IMGUI
    // ImGui操作の更新
    void UpdateImGui();

    void UpdateImGui_GlobalSettings();
    void UpdateImGui_LightSettings();
    void UpdateImGui_GameCamera();
    void UpdateImGui_Object3d();
    void UpdateImGui_Particle();
    void UpdateImGui_Sprite();
    void UpdateImGui_Sound();
	void UpdateImGui_Skybox();
    void UpdateImGui_HelpWindow();
#endif // USE_IMGUI

private:
    // カメラ
    std::unique_ptr<Camera> camera_;

    DebugCamera debugCamera_;
    bool useDebugCamera_ = false;
    Vector3 gameCameraRotate_{};
    Vector3 gameCameraTranslate_{};

    // ゲームオブジェクト
	std::unique_ptr<Skybox> skybox_;
    std::vector < std::unique_ptr<Object3d>> object3ds_;
    std::vector<std::unique_ptr<Sprite>> sprites_;

    // レベルデータ
    std::unique_ptr<LevelData> levelData_;

    // 設定
	bool isShowParticle_ = true;    // パーティクルの表示フラグ
	bool isShowSkybox_ = true;      // スカイボックスの表示フラグ
	bool isShowMaterial_ = true;    // マテリアルを持つオブジェクトの表示フラグ
	bool isShowSprite_ = false;     // スプライトの表示フラグ
	bool isUpdateParticle_ = false; // パーティクルの更新フラグ
	bool useBillboard_ = false;      // パーティクルのビルボード使用フラグ
	int objectControlIndex_ = 0;    // ImGuiで操作するオブジェクトのインデックス

	// ブレンドモード
    // 描画に使用するブレンドモードのインデックス（ImGuiで操作する用）
	int currentBlendMode_ = BlendMode::BlendState::kBlendModeNormal;
	// パーティクルのブレンドモードのインデックス（ImGuiで操作する用）
    int particleBlendMode_ = BlendMode::BlendState::kBlendModeAdd;

    // パス定数
    // 3Dモデルのファイルパス
    const std::string planeModel_ = "plane.obj";
    const std::string planeGltfModel_ = "plane.gltf";
    const std::string teapotModel_ = "teapot.obj";
    const std::string terrainModel_ = "terrain.obj";
    const std::string sphereModel_ = "sphere.obj";
    // テクスチャファイルパスを保持
    const std::string uvCheckerPath_ = "uvChecker.png";
    const std::string monsterBallPath_ = "monsterBall.png";
    const std::string grassPath_ = "grass.png";
    
    // パーティクルグループの作成
    const std::string particleCirclePath_ = "Particles/circle.png";
	const std::string particleCircle2Path_ = "Particles/circle2.png";
	const std::string particleGradationLinePath_ = "Particles/gradationLine.png";
    const std::string particleGroupName_ = "TestGroup";
	const std::string cylinderParticleGroupName_ = "RingGroup";
	const std::string ringParticleGroupName_ = "RingShapeGroup";
};