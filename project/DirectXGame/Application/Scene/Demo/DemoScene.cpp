#include "DemoScene.h"
#include "Win32Window.h"
#include "DX12Context.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include "ParticleManager.h"
#include "AudioManager.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "LightManager.h"
#include "SceneManager.h"
#include "ApplicationConfig.h" // kDeltaTimeなど
// 計算用関数など
#include "MathUtils.h"
#include "MatrixGenerators.h"

// using
using namespace MathUtils;
using namespace MathGenerators;
using namespace BlendMode;

void DemoScene::Initialize() {
	// カメラ生成
	camera_ = std::make_unique<Camera>();
	camera_->Initialize();
	camera_->SetRotate(kDefaultCameraRotate);
	camera_->SetTranslate(kDefaultCameraTranslate);
	gameCameraRotate_ = camera_->GetRotate();

	gameCameraTranslate_ = camera_->GetTranslate();

	debugCamera_.Initialize();

	Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());

	// ライトの設定
	LightManager::GetInstance()->SetDirectionalLightIntensity(0.0f);
	LightManager::GetInstance()->SetPointLightIntensity(0.0f);

	// テクスチャ読み込み
	TextureManager::GetInstance()->LoadTexture(uvCheckerPath_);
	TextureManager::GetInstance()->LoadTexture(monsterBallPath_);
	TextureManager::GetInstance()->LoadTexture(grassPath_);
	// パーティクルテクスチャの読み込み
	TextureManager::GetInstance()->LoadTexture(particleCirclePath_);
	TextureManager::GetInstance()->LoadTexture(particleCircle2Path_);
	TextureManager::GetInstance()->LoadTexture(particleGradationLinePath_);

	// モデル読み込み
	ModelManager::GetInstance()->LoadModel("sphere", sphereModel_);
	ModelManager::GetInstance()->LoadModel("plane", planeGltfModel_);

	// パーティクル設定
	ParticleManager::GetInstance()->CreateParticleGroup(particleGroupName_, particleGradationLinePath_);
	ParticleManager::GetInstance()->CreateParticleGroup(cylinderParticleGroupName_, particleGradationLinePath_);
	ParticleManager::GetInstance()->CreateParticleGroup(ringParticleGroupName_, particleCirclePath_);

	// エミッターの設定と登録
	ParticleEmitter defaultEmitter = ParticleEmitter(
		Transform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {-1.0f, 1.0f, 0.0f} },
		1,   // count
		0.2f // frequency:
	);
	ParticleManager::GetInstance()->SetEmitter(particleGroupName_, defaultEmitter);

	// シリンダーエミッターの設定
	ParticleEmitter cylinderEmitter = ParticleEmitter(
		Transform{ {1.0f, 1.0f, 1.0f}, {-0.3f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f} },
		1,
		0.4f
	);
	cylinderEmitter.SetIsEffectMode(true);
	cylinderEmitter.SetIsLoop(true);
	{
		ParticleGenerateSettings gs = cylinderEmitter.GetGenerateSettings();
		gs.isRandomScale = false;
		gs.rotateMin = { 0.0f, 0.0f, 0.0f };
		gs.rotateMax = { 0.0f, 1.0f, 0.0f };
		cylinderEmitter.SetGenerateSettings(gs);

		ParticleUVAnimationSettings uvas = cylinderEmitter.GetUVAnimationSettings();
		uvas.isActive = true;
		uvas.scrollSpeed = { 0.3f, 0.0f };
		cylinderEmitter.SetUVAnimationSettings(uvas);
	}
	cylinderEmitter.SetShapeType(ParticleShapeType::Cylinder);
	if (auto* cs = cylinderEmitter.GetCylinderShape()) {
		CylinderSettings csSettings = cs->GetSettings();
		csSettings.flipV = true;
		cs->SetSettings(csSettings);
	}
	ParticleManager::GetInstance()->SetEmitter(cylinderParticleGroupName_, cylinderEmitter);

	// リングエミッターの設定
	ParticleEmitter ringEmitter = ParticleEmitter(
		Transform{ {1.0f, 1.0f, 1.0f}, {0.3f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} },
		1,
		0.4f
	);
	ringEmitter.SetIsEffectMode(true);
	ringEmitter.SetIsLoop(true);
	{
		ParticleGenerateSettings gs = ringEmitter.GetGenerateSettings();
		gs.isRandomScale = false;
		ringEmitter.SetGenerateSettings(gs);

		ParticleUVAnimationSettings uvas = ringEmitter.GetUVAnimationSettings();
		uvas.isActive = true;
		uvas.scrollSpeed = { 0.1f, 0.0f };
		ringEmitter.SetUVAnimationSettings(uvas);
	}
	ringEmitter.SetShapeType(ParticleShapeType::Ring);
	if (auto* rs = ringEmitter.GetRingShape()) {
		RingSettings rsSettings = rs->GetSettings();
		rsSettings.innerRadius = 0.5f;
		rsSettings.startOuterRadius = 1.0f;
		rsSettings.midOuterRadius = 1.2f;
		rsSettings.endOuterRadius = 1.5f;
		rs->SetSettings(rsSettings);
	}
	ParticleManager::GetInstance()->SetEmitter(ringParticleGroupName_, ringEmitter);

	// 音声ファイルをロード
	if (!AudioManager::GetInstance()->LoadSound("alarm1", "Alarm01.mp3")) {
		// ロード失敗時の処理
		assert(false && "Failed to load sound: Alarm01.mp3");
	}

	// Skyboxの設定
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize("skybox.dds");
	skybox_->SetCamera(camera_.get());

	// 環境マップとしてセット
	TextureManager::GetInstance()->LoadTexture("skybox.dds");
	Object3dCommon::GetInstance()->SetEnvironmentMap(TextureManager::GetInstance()->GetSrvIndex("skybox.dds"));

	// --- 3Dオブジェクト生成 ---
	// マテリアル

	// 1つ目のオブジェクト
	std::unique_ptr <Object3d> object3d_1 = std::make_unique<Object3d>();
	object3d_1->Initialize();

	// モデルの設定
	object3d_1->SetModel(sphereModel_);
	// オブジェクトの位置を設定
	object3d_1->SetTranslate({ 0.0f, 0.0f, 0.0f });

	// 2つ目のオブジェクトを新規作成
	std::unique_ptr <Object3d> object3d_2 = std::make_unique<Object3d>();
	object3d_2->Initialize();

	// モデルの設定
	object3d_2->SetModel(planeGltfModel_);
	// オブジェクトの位置を設定
	object3d_2->SetTranslate({ 2.0f, 0.0f, 0.0f });

	// Object3dを格納する
	object3ds_.push_back(std::move(object3d_1));
	object3ds_.push_back(std::move(object3d_2));

	// --- スプライト生成 ---
	// 描画サイズ
	const float spriteCutSize = 64.0f;
	const float spriteScale = 64.0f;
	// 生成
	for (uint32_t i = 0; i < 10; ++i) {
		std::unique_ptr<Sprite> newSprite = std::make_unique<Sprite>();
		newSprite->Initialize(uvCheckerPath_);
		newSprite->SetAnchorPoint({ 0.5f, 0.5f });
		if (i == 7) {
			newSprite->SetFlipX(1);
			newSprite->SetFlipY(1);
		}
		else if (i == 8) {
			newSprite->SetFlipX(1);
			newSprite->SetTextureLeftTop({ 0.0f, 0.0f });
			newSprite->SetTextureSize({ spriteCutSize, spriteCutSize });
			newSprite->SetScale({ spriteScale, spriteScale });
		}
		else if (i == 9) {
			newSprite->SetTextureLeftTop({ 0.0f, 0.0f });
			newSprite->SetTextureSize({ spriteCutSize, spriteCutSize });
			newSprite->SetScale({ spriteScale, spriteScale });
		}
		newSprite->SetTranslate(
			{ float(i * spriteScale / 3), float(i * spriteScale / 3) });
		sprites_.push_back(std::move(newSprite));
	}

	// --- レベルデータ読み込み ---
	// ※ 実際のJSONファイルがある場合は、ファイル名を指定して読み込み
	// levelData_ = LevelLoader::LoadFile("testScene");
	if (levelData_) {
		CreateObjects(levelData_->objects);
		CreatePlayerSpawns(levelData_->players);
	}
}

void DemoScene::Finalize() {
	// Object3dCommonの参照をクリア
	Object3dCommon::GetInstance()->SetDefaultCamera(nullptr);
	// Object3dを削除
	object3ds_.clear();

	// スプライトを削除
	sprites_.clear();
}

void DemoScene::Update() {
	// 入力の更新
	Input::GetInstance()->Update();

#ifdef USE_IMGUI
	// 3. UI処理 (ImGuiの定義)
	UpdateImGui();
#endif // USE_IMGUI

	// 4. ゲームロジックの更新

	// 音声のクリーンアップ
	AudioManager::GetInstance()->CleanupFinishedVoices();

	// カメラの更新処理
	UpdateGameCamera();

	// Skyboxの更新
	if (skybox_) {
		skybox_->Update();
	}

	// パーティクルの更新
	// 状態を反映
	ParticleManager::GetInstance()->SetIsUpdate(isUpdateParticle_);
	ParticleManager::GetInstance()->SetUseBillboard(useBillboard_);

	// ※ kDeltaTime は ApplicationConfig.h
	// から読み込むか、メンバ変数として管理する
	ParticleManager::GetInstance()->Update(*camera_, ApplicationConfig::kDeltaTime);


	if (!object3ds_.empty()) {
		// オブジェクトの操作
		if (!useDebugCamera_ && isShowMaterial_) {
			Vector3 pos = object3ds_[objectControlIndex_]->GetTranslate();
			Vector3 rot = object3ds_[objectControlIndex_]->GetRotation();
			float moveSpeed = kMoveSpeed;
			float rotSpeed = kRotateSpeed;

			if (Input::GetInstance()->IsKeyDown(DIK_A)) {
				pos.x -= moveSpeed;
			}
			if (Input::GetInstance()->IsKeyDown(DIK_D)) {
				pos.x += moveSpeed;
			}
			if (Input::GetInstance()->IsKeyDown(DIK_W)) {
				pos.z += moveSpeed;
			}
			if (Input::GetInstance()->IsKeyDown(DIK_S)) {
				pos.z -= moveSpeed;
			}
			if (Input::GetInstance()->IsKeyDown(DIK_Q)) {
				pos.y += moveSpeed;
			}
			if (Input::GetInstance()->IsKeyDown(DIK_E)) {
				pos.y -= moveSpeed;
			}
			if (Input::GetInstance()->IsKeyDown(DIK_Z)) {
				rot.y -= rotSpeed;
			}
			if (Input::GetInstance()->IsKeyDown(DIK_C)) {
				rot.y += rotSpeed;
			}
			// 共通のリセットキー
			if (Input::GetInstance()->IsKeyDown(DIK_R)) {
				pos = { 0.0f, 0.0f, 0.0f };
				rot = { 0.0f, 0.0f, 0.0f };
			}

			object3ds_[objectControlIndex_]->SetTranslate(pos);
			object3ds_[objectControlIndex_]->SetRotation(rot);
		}
		// オブジェクトの更新
		for (const auto& obj : object3ds_) {
			obj->Update();
		}
	}


	// スプライトの更新
	for (const auto& sprite : sprites_) {
		sprite->Update();
	}
}

void DemoScene::Draw() {
	// MyGame::Draw の中身を移動
	// ※ dxBase_->PreDraw() などは MyGame 側で行うので、ここでは「シーンの中身の描画」だけを行う

	// 描画設定
	Object3dCommon::GetInstance()->SetCommonDrawSettings(
		static_cast<BlendState>(currentBlendMode_));


	// 2. 3Dオブジェクトの描画

	if (isShowMaterial_ && !object3ds_.empty()) {
		for (const auto& obj : object3ds_) {
			obj->Draw();
		}
	}

	// Skyboxの描画(object3dの描画が終わった後に描画するのが基本)
	if (skybox_ && isShowSkybox_) {
		skybox_->Draw();
	}

	// 3. パーティクルの描画
	if (isShowParticle_) {
		ParticleManager::GetInstance()->Draw(
			static_cast<BlendState>(particleBlendMode_));
	}


	// 4. スプライトの描画
	// 描画設定
	SpriteCommon::GetInstance()->SetCommonDrawSettings(
		static_cast<BlendState>(currentBlendMode_));

	if (isShowSprite_) {
		for (size_t i = 0; i < sprites_.size(); ++i) {
			// テクスチャの差し替え（特定の要素だけ変える処理）
			if (i == 6) {
				// ※ パスを持っているならここでセット
				sprites_[i]->SetTexture(monsterBallPath_);
			}
			sprites_[i]->Draw();
		}
	}
}

void DemoScene::CreateObjects(const std::vector<LevelData::ObjectData>& data) {
	for (const auto& objData : data) {
		// MESHタイプの場合のみ、エンジンの3Dオブジェクトをインスタンス化
		if (objData.type == "MESH") {
			std::unique_ptr<Object3d> newObj = std::make_unique<Object3d>();
			newObj->Initialize();

			// モデルの設定 (ModelManager等で事前にロードされている必要がある、または内部でロードされる)
			newObj->SetModel(objData.fileName);

			// ローダーで変換済みのトランスフォームを適用
			newObj->SetTranslate(objData.translation);
			newObj->SetRotation(objData.rotation);
			newObj->SetScale(objData.scaling);

			// リストに追加
			object3ds_.push_back(std::move(newObj));
		}

		// 子要素が存在する場合は再帰的に処理
		if (!objData.children.empty()) {
			CreateObjects(objData.children);
		}
	}
}

void DemoScene::UpdateGameCamera() {

	if (Input::GetInstance()->IsKeyTriggered(DIK_F1)) {
		useDebugCamera_ = !useDebugCamera_;
	}
	if (!useDebugCamera_) {
		camera_->SetRotate(kDefaultCameraRotate);
	}

	if (!useDebugCamera_) {
		// キー入力によるカメラ操作
		if (Input::GetInstance()->IsKeyDown(DIK_LEFT)) {
			gameCameraTranslate_.x -= kMoveSpeed;
		}
		if (Input::GetInstance()->IsKeyDown(DIK_RIGHT)) {
			gameCameraTranslate_.x += kMoveSpeed;
		}
		if (Input::GetInstance()->IsKeyDown(DIK_UP)) {
			gameCameraTranslate_.y += kMoveSpeed;
		}
		if (Input::GetInstance()->IsKeyDown(DIK_DOWN)) {
			gameCameraTranslate_.y -= kMoveSpeed;
		}
		// 共通のリセットキー
		if (Input::GetInstance()->IsKeyDown(DIK_R)) {
			camera_->SetRotate(kDefaultCameraRotate);
			gameCameraTranslate_ = kDefaultCameraTranslate;
		}

		camera_->SetTranslate(gameCameraTranslate_);
	}
	else {
		debugCamera_.Update();
		camera_->SetRotate(debugCamera_.GetRotation());
		camera_->SetTranslate(debugCamera_.GetTranslate());
	}
	camera_->Update();
}

#ifdef USE_IMGUI

void DemoScene::UpdateImGui() {
	// メイン設定ウィンドウの位置とサイズ
	ImGui::SetNextWindowPos(ImVec2(Win32Window::kClientWidth - 10.0f, 10.0f), ImGuiCond_Once, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(450.0f, 600.0f), ImGuiCond_Once);

	if (ImGui::Begin("Settings")) {
		UpdateImGui_GlobalSettings();
		UpdateImGui_LightSettings();
		UpdateImGui_GameCamera();
		UpdateImGui_Object3d();
		UpdateImGui_Particle();
		UpdateImGui_Sprite();
		UpdateImGui_Sound();
		UpdateImGui_Skybox();
	}
		ImGui::End(); // "Settings" ウィンドウの終了

	// ヘルプウィンドウの表示
	UpdateImGui_HelpWindow();
}

void DemoScene::UpdateImGui_GlobalSettings() {
	if (ImGui::TreeNode("Global Settings")) {
		ImGui::Checkbox("useDebugCamera (F1)", &useDebugCamera_);
		ImGui::Checkbox("showParticle", &isShowParticle_);
		ImGui::Checkbox("showMaterial", &isShowMaterial_);
		ImGui::Checkbox("showSprite", &isShowSprite_);

		// オブジェクト選択コンボボックス
		std::string currentLabel = "Object " + std::to_string(objectControlIndex_);
		if (ImGui::BeginCombo("Select Object", currentLabel.c_str())) {
			for (size_t i = 0; i < object3ds_.size(); ++i) {
				// 各項目のラベルを作成
				std::string label = "Object " + std::to_string(i);
				// 現在選択されている項目かどうかを判定
				const bool isSelected = (objectControlIndex_ == static_cast<int>(i));
				// 選択肢を表示
				if (ImGui::Selectable(label.c_str(), isSelected)) {
					// クリックされたらインデックスを更新
					objectControlIndex_ = static_cast<int>(i);
				}
				// 最初から選択されている項目にスクロールを合わせる
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Combo("BlendMode", &currentBlendMode_, "None\0Normal\0Add\0Subtractive\0Multiply\0Screen\0");
		ImGui::TreePop();
	}
}

void DemoScene::UpdateImGui_LightSettings() {
	ImGui::Separator();
	if (ImGui::TreeNode("Light Settings")) {
		// --- Directional Light ---
		if (ImGui::TreeNode("DirectioalLight_")) {
			DirectionalLight lightData = LightManager::GetInstance()->GetDirectionalLightData();
			bool changed = false;
			changed |= ImGui::ColorEdit4("color", &lightData.color.x);
			changed |= ImGui::DragFloat3("direction", &lightData.direction.x, 0.01f);
			changed |= ImGui::DragFloat("intensity", &lightData.intensity, 0.01f);
			if (changed) {
				LightManager::GetInstance()->SetDirectionalLightData(lightData);
			}
			ImGui::TreePop();
		}
		ImGui::Separator();

		// --- Point Light ---
		if (ImGui::TreeNode("PointLight_")) {
			PointLight pointLight = LightManager::GetInstance()->GetPointLightData();
			bool changed = false;
			changed |= ImGui::ColorEdit4("color", &pointLight.color.x);
			changed |= ImGui::DragFloat3("position", &pointLight.position.x, 0.01f);
			changed |= ImGui::DragFloat("intensity", &pointLight.intensity, 0.01f);
			changed |= ImGui::DragFloat("radius", &pointLight.radius, 0.01f);
			changed |= ImGui::DragFloat("decay", &pointLight.decay, 0.01f);
			if (changed) {
				LightManager::GetInstance()->SetPointLightData(pointLight);
			}
			ImGui::TreePop();
		}
		ImGui::Separator();

		// --- Spot Light ---
		if (ImGui::TreeNode("SpotLight_")) {
			SpotLight spotLight = LightManager::GetInstance()->GetSpotLightData();
			bool changed = false;
			changed |= ImGui::ColorEdit4("color", &spotLight.color.x);
			changed |= ImGui::DragFloat3("position", &spotLight.position.x, 0.01f);
			changed |= ImGui::DragFloat("distance", &spotLight.distance, 0.01f);
			changed |= ImGui::DragFloat3("direction", &spotLight.direction.x, 0.01f);
			changed |= ImGui::DragFloat("intensity", &spotLight.intensity, 0.01f);
			changed |= ImGui::DragFloat("decay", &spotLight.decay, 0.01f);
			changed |= ImGui::DragFloat("cosAngle", &spotLight.cosAngle, 0.01f);
			changed |= ImGui::DragFloat("cosFalloffStart", &spotLight.cosFalloffStart, 0.01f);
			if (changed) {
				LightManager::GetInstance()->SetSpotLightData(spotLight);
			}
			ImGui::TreePop();
		}
		ImGui::TreePop();
	}
}

void DemoScene::UpdateImGui_GameCamera() {
	ImGui::Separator();
	if (ImGui::TreeNode("GameCamera")) {
		ImGui::DragFloat3("rotate", &gameCameraRotate_.x, 0.01f);
		ImGui::DragFloat3("translate", &gameCameraTranslate_.x, 0.01f);
		ImGui::TreePop();
	}
}

void DemoScene::UpdateImGui_Object3d() {
	ImGui::Separator();
	if (ImGui::TreeNode("Object3d")) {
		ImGui::Checkbox("showMaterial", &isShowMaterial_);

		for (size_t i = 0; i < object3ds_.size(); ++i) {
			// ループ内のID衝突を防ぐ
			ImGui::PushID(static_cast<int>(i));
			// 現在のオブジェクトを取得
			Object3d* object3d = object3ds_[i].get();
			// ノードのラベルを動的に生成
			std::string label = "Object3d " + std::to_string(i + 1);

			if (ImGui::TreeNode(label.c_str())) {
				Vector3 scale = object3d->GetScale();
				if (ImGui::DragFloat3("scale", &scale.x, 0.01f)) {
					object3d->SetScale(scale);
				}
				Vector3 rotate = object3d->GetRotation();
				if (ImGui::DragFloat3("rotate", &rotate.x, 0.01f)) {
					object3d->SetRotation(rotate);
				}
				Vector3 translate = object3d->GetTranslate();
				if (ImGui::DragFloat3("translate", &translate.x, 0.01f)) {
					object3d->SetTranslate(translate);
				}

				if (Model* model = object3d->GetModel()) {
					ImGui::Separator();
					bool enableLighting = model->IsEnableLighting();

					Vector4 color = model->GetColor();
					if (ImGui::ColorEdit4("color", &color.x)) {
						model->SetColor(color);
					}
					if (ImGui::Checkbox("enableLighting", &enableLighting)) {
						model->SetEnableLighting(enableLighting);
					}

					float environmentCoefficient = model->GetEnvironmentCoefficient();
					if (ImGui::DragFloat("environmentCoefficient", &environmentCoefficient, 0.01f, 0.0f, 1.0f)) {
						model->SetEnvironmentCoefficient(environmentCoefficient);
					}

					ImGui::Separator();
					ImGui::Text("Dissolve");
					bool enableDissolve = model->GetDissolveEnable() != 0;
					float dissolveThreshold = model->GetDissolveThreshold();
					float dissolveEdgeRange = model->GetDissolveEdgeRange();
					Vector3 dissolveEdgeColor = model->GetDissolveEdgeColor();
					std::string currentMask = model->GetDissolveMaskTexturePath();
					int maskIndex = (currentMask == "masks/noise1.png") ? 1 : 0;
					const char* maskPaths[] = { "masks/noise0.png", "masks/noise1.png" };
					const char* maskNames[] = { "noise0", "noise1" };

					bool dissolveChanged = false;
					if (ImGui::Checkbox("Enable Dissolve", &enableDissolve)) { dissolveChanged = true; }
					if (ImGui::SliderFloat("Threshold", &dissolveThreshold, 0.0f, 1.0f)) { dissolveChanged = true; }
					if (ImGui::SliderFloat("Edge Range", &dissolveEdgeRange, 0.0f, 0.2f)) { dissolveChanged = true; }
					float colorArr[3] = { dissolveEdgeColor.x, dissolveEdgeColor.y, dissolveEdgeColor.z };
					if (ImGui::ColorEdit3("Edge Color", colorArr)) {
						dissolveEdgeColor = { colorArr[0], colorArr[1], colorArr[2] };
						dissolveChanged = true;
					}
					if (ImGui::Combo("Mask Texture", &maskIndex, maskNames, 2)) {
						model->SetDissolveMaskTexture(maskPaths[maskIndex]);
					}
					if (dissolveChanged) {
						model->SetDissolveParams(enableDissolve ? 1 : 0, dissolveThreshold, dissolveEdgeRange, dissolveEdgeColor);
					}
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
}

void DemoScene::UpdateImGui_Particle() {
	ImGui::Separator();
	if (ImGui::TreeNode("Particle")) {
		ImGui::Checkbox("update", &isUpdateParticle_);
		ImGui::Checkbox("useBillboard", &useBillboard_);
		ImGui::Combo("ParticleBlendMode", &particleBlendMode_, "None\0Normal\0Add\0Subtractive\0Multiply\0Screen\0");

		ImGui::Separator();

		// 安全対策用ラムダ式ヘルパー
		auto DragFloatMinMax = [](const char* labelMin, const char* labelMax, float* minVal, float* maxVal, float speed = 0.01f) {
			if (ImGui::DragFloat(labelMin, minVal, speed, 0.0f, *maxVal)) { if (*minVal > *maxVal) *minVal = *maxVal; }
			if (ImGui::DragFloat(labelMax, maxVal, speed, *minVal, 100.0f)) { if (*maxVal < *minVal) *maxVal = *minVal; }
			};
		auto DragFloat3MinMax = [](const char* labelMin, const char* labelMax, Vector3* minVal, Vector3* maxVal, float speed = 0.01f) {
			ImGui::DragFloat3(labelMin, &minVal->x, speed); ImGui::DragFloat3(labelMax, &maxVal->x, speed);
			if (minVal->x > maxVal->x) minVal->x = maxVal->x;
			if (minVal->y > maxVal->y) minVal->y = maxVal->y;
			if (minVal->z > maxVal->z) minVal->z = maxVal->z;
			};

		// 登録されている全パーティクルグループを取得して個別に設定
		std::vector<std::string> groupNames = ParticleManager::GetInstance()->GetParticleGroupNames();
		for (const auto& groupName : groupNames) {
			ImGui::PushID(groupName.c_str());

			if (ImGui::TreeNode(groupName.c_str())) {
				ParticleEmitter* emitter = ParticleManager::GetInstance()->GetEmitter(groupName);
				if (emitter) {
					Vector3 translate = emitter->GetTranslate();
					if (ImGui::DragFloat3("Translate", &translate.x, 0.01f)) { emitter->SetTranslate(translate); }
					Vector3 rotate = emitter->GetRotate();
					if (ImGui::DragFloat3("Rotate", &rotate.x, 0.01f)) { emitter->SetRotate(rotate); }
					Vector3 scale = emitter->GetScale();
					if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) { emitter->SetScale(scale); }

					int count = static_cast<int>(emitter->GetCount());
					if (ImGui::DragInt("Count", &count, 1, 1, 100)) { emitter->SetCount(static_cast<uint32_t>(count)); }

					// テクスチャ選択
					std::string currentTex = ParticleManager::GetInstance()->GetGroupTexture(groupName);
					const char* textureItems[] = { particleCirclePath_.c_str(), particleCircle2Path_.c_str(), particleGradationLinePath_.c_str() };
					constexpr int textureCount = sizeof(textureItems) / sizeof(textureItems[0]);
					int currentTexIndex = -1;
					for (int i = 0; i < textureCount; ++i) {
						if (currentTex == textureItems[i]) { currentTexIndex = i; break; }
					}
					if (ImGui::Combo("Texture", &currentTexIndex, textureItems, textureCount)) {
						if (currentTexIndex >= 0 && currentTexIndex < textureCount) {
							ParticleManager::GetInstance()->SetGroupTexture(groupName, textureItems[currentTexIndex]);
						}
					}

					bool isEmit = emitter->IsEmit();
					if (ImGui::Checkbox("Enable Auto Emit", &isEmit)) { emitter->SetIsEmit(isEmit); }
					float frequency = emitter->GetFrequency();
					if (ImGui::DragFloat("Frequency", &frequency, 0.01f, 0.01f, 10.0f)) { emitter->SetFrequency(frequency); }

					bool isEffectMode = emitter->IsEffectMode();
					if (ImGui::Checkbox("Effect Mode (Single/Loop Effect)", &isEffectMode)) { emitter->SetIsEffectMode(isEffectMode); }
					if (emitter->IsEffectMode()) {
						bool isLoop = emitter->IsLoop();
						if (ImGui::Checkbox("Loop Effect", &isLoop)) { emitter->SetIsLoop(isLoop); }
						ImGui::Text("Playing State: %s", emitter->IsPlaying() ? "Playing" : "Stopped");
					}

					// Generate Settings
					if (ImGui::TreeNode("Generate Settings")) {
						ParticleGenerateSettings gs = emitter->GetGenerateSettings();
						bool changed = false;
						// Scale
						changed |= ImGui::Checkbox("Random Scale", &gs.isRandomScale);
						if (gs.isRandomScale) { DragFloat3MinMax("Scale Min", "Scale Max", &gs.scaleMin, &gs.scaleMax); changed = true; }
						else { changed |= ImGui::DragFloat3("Fixed Scale", &gs.fixedScale.x, 0.01f); }
						// Rotate
						changed |= ImGui::Checkbox("Random Rotate", &gs.isRandomRotate);
						if (gs.isRandomRotate) { DragFloat3MinMax("Rotate Min", "Rotate Max", &gs.rotateMin, &gs.rotateMax); changed = true; }
						else { changed |= ImGui::DragFloat3("Fixed Rotate", &gs.fixedRotate.x, 0.01f); }
						// Velocity
						changed |= ImGui::Checkbox("Random Velocity", &gs.isRandomVelocity);
						if (gs.isRandomVelocity) { DragFloat3MinMax("Velocity Min", "Velocity Max", &gs.velocityMin, &gs.velocityMax); changed = true; }
						else { changed |= ImGui::DragFloat3("Fixed Velocity", &gs.fixedVelocity.x, 0.01f); }
						// LifeTime
						changed |= ImGui::Checkbox("Random LifeTime", &gs.isRandomLifeTime);
						if (gs.isRandomLifeTime) { DragFloatMinMax("LifeTime Min", "LifeTime Max", &gs.lifeTimeMin, &gs.lifeTimeMax); changed = true; }
						else { changed |= ImGui::DragFloat("Fixed LifeTime", &gs.fixedLifeTime, 0.01f); }
						// Color
						changed |= ImGui::Checkbox("Random Color", &gs.isRandomColor);
						if (gs.isRandomColor) {
							changed |= ImGui::ColorEdit4("Color Min", &gs.colorMin.x);
							changed |= ImGui::ColorEdit4("Color Max", &gs.colorMax.x);
						}
						else { changed |= ImGui::ColorEdit4("Fixed Color", &gs.fixedColor.x); }
						if (changed) { emitter->SetGenerateSettings(gs); }
						ImGui::TreePop();
					}

					// Field Settings
					if (ImGui::TreeNode("Field Settings")) {
						ParticleFieldSettings fs = emitter->GetFieldSettings();
						bool changed = false;
						// Acceleration Field
						changed |= ImGui::Checkbox("Acceleration Field Active", &fs.isAccelerationFieldActive);
						if (fs.isAccelerationFieldActive) {
							changed |= ImGui::DragFloat3("Acceleration", &fs.accelerationField.acceleration.x, 0.1f);
							DragFloat3MinMax("Area Min", "Area Max", &fs.accelerationField.area.min, &fs.accelerationField.area.max, 0.1f);
							changed = true;
						}
						ImGui::Separator();
						// Gravity Field
						changed |= ImGui::Checkbox("Gravity Field Active", &fs.isGravityFieldActive);
						if (fs.isGravityFieldActive) { changed |= ImGui::DragFloat3("Gravity Vector", &fs.gravity.x, 0.1f); }
						if (changed) { emitter->SetFieldSettings(fs); }
						ImGui::TreePop();
					}

					// UV Animation Settings
					if (ImGui::TreeNode("UV Animation Settings")) {
						ParticleUVAnimationSettings uvas = emitter->GetUVAnimationSettings();
						bool changed = false;
						changed |= ImGui::Checkbox("UV Animation Active", &uvas.isActive);
						if (uvas.isActive) {
							changed |= ImGui::Checkbox("Individual UV Animation (Effect Mode)", &uvas.isIndividual);
							changed |= ImGui::DragFloat2("Scroll Speed (U, V)", &uvas.scrollSpeed.x, 0.01f);
							changed |= ImGui::DragFloat("Rotate Speed (rad/s)", &uvas.rotateSpeed, 0.01f);
							changed |= ImGui::DragFloat2("Scale Speed (U, V)", &uvas.scaleSpeed.x, 0.01f);

							if (ImGui::Button("Reset UV States")) {
								uvas.currentTranslate = { 0.0f, 0.0f }; uvas.currentRotate = 0.0f; uvas.currentScale = { 1.0f, 1.0f };
								changed = true;
							}
						}
						if (changed) { emitter->SetUVAnimationSettings(uvas); }
						ImGui::TreePop();
					}

					// ============================================================
					// 形状切り替えコンボボックス
					// ============================================================
					const char* shapeItems[] = { "Billboard", "Ring", "Cylinder", "Plane" };
					int shapeIndex = static_cast<int>(emitter->GetShapeType());
					if (ImGui::Combo("Shape Type", &shapeIndex, shapeItems, 4)) {
						emitter->SetShapeType(static_cast<ParticleShapeType>(shapeIndex));
					}

					// --- Ring Shape Settings ---
					if (auto* rs = emitter->GetRingShape()) {
						if (ImGui::TreeNode("Ring Shape Settings")) {
							RingSettings s = rs->GetSettings();
							bool changed = false;

							// 元のコードの全設定項目を完全網羅
							float tempInner = s.innerRadius;
							float tempOuter = s.startOuterRadius;
							DragFloatMinMax("Inner Radius", "Start Outer Radius", &tempInner, &tempOuter);
							if (tempInner != s.innerRadius || tempOuter != s.startOuterRadius) {
								s.innerRadius = tempInner;
								s.startOuterRadius = tempOuter;
								changed = true;
							}

							changed |= ImGui::DragFloat("Mid Outer Radius", &s.midOuterRadius, 0.01f, 0.0f, 20.0f);
							changed |= ImGui::DragFloat("End Outer Radius", &s.endOuterRadius, 0.01f, 0.0f, 20.0f);

							changed |= ImGui::DragFloat("Start Angle (deg)", &s.startAngle, 1.0f);
							changed |= ImGui::DragFloat("End Angle (deg)", &s.endAngle, 1.0f);

							int division = static_cast<int>(s.division);
							if (ImGui::DragInt("Division", &division, 1, 3, 256)) {
								s.division = static_cast<uint32_t>(division);
								changed = true;
							}

							ImGui::Separator();
							changed |= ImGui::Checkbox("Swap UV (U <-> V)", &s.isUvSwap);
							changed |= ImGui::ColorEdit4("Inner Vertex Color", &s.innerColor.x);
							changed |= ImGui::ColorEdit4("Outer Vertex Color", &s.outerColor.x);
							changed |= ImGui::DragFloat("Fade Start Alpha", &s.fadeStartAlpha, 0.01f, 0.0f, 1.0f);
							changed |= ImGui::DragFloat("Fade End Alpha", &s.fadeEndAlpha, 0.01f, 0.0f, 1.0f);
							changed |= ImGui::DragFloat("Fade Range", &s.fadeRange, 0.01f, 0.0f, 0.5f);

							if (changed) {
								rs->SetSettings(s);
							}
							ImGui::TreePop();
						}
					}

					// --- Cylinder Shape Settings ---
					if (auto* cs = emitter->GetCylinderShape()) {
						if (ImGui::TreeNode("Cylinder Shape Settings")) {
							CylinderSettings s = cs->GetSettings();
							bool changed = false;

							changed |= ImGui::DragFloat("Height", &s.height, 0.01f, 0.0f, 50.0f);
							changed |= ImGui::DragFloat2("Top Radius (X, Z)", &s.topRadius.x, 0.01f, 0.0f, 20.0f);
							changed |= ImGui::DragFloat2("Bottom Radius (X, Z)", &s.bottomRadius.x, 0.01f, 0.0f, 20.0f);

							changed |= ImGui::DragFloat("Start Angle (deg)", &s.startAngle, 1.0f);
							changed |= ImGui::DragFloat("End Angle (deg)", &s.endAngle, 1.0f);

							int division = static_cast<int>(s.division);
							if (ImGui::DragInt("Division", &division, 1, 3, 256)) {
								s.division = static_cast<uint32_t>(division);
								changed = true;
							}
							int verticalDivision = static_cast<int>(s.verticalDivision);
							if (ImGui::DragInt("Vertical Division", &verticalDivision, 1, 1, 256)) {
								s.verticalDivision = static_cast<uint32_t>(verticalDivision);
								changed = true;
							}

							ImGui::Separator();
							changed |= ImGui::Checkbox("Swap UV (U <-> V)", &s.isUvSwap);
							changed |= ImGui::Checkbox("Flip V", &s.flipV);
							changed |= ImGui::ColorEdit4("Top Vertex Color", &s.topColor.x);
							changed |= ImGui::ColorEdit4("Bottom Vertex Color", &s.bottomColor.x);
							changed |= ImGui::DragFloat("Fade Start Alpha", &s.fadeStartAlpha, 0.01f, 0.0f, 1.0f);
							changed |= ImGui::DragFloat("Fade End Alpha", &s.fadeEndAlpha, 0.01f, 0.0f, 1.0f);
							changed |= ImGui::DragFloat("Fade Range", &s.fadeRange, 0.01f, 0.0f, 0.5f);
							changed |= ImGui::DragFloat("Alpha Reference (Discard)", &s.alphaReference, 0.01f, 0.0f, 1.0f);

							if (changed) {
								cs->SetSettings(s);
							}
							ImGui::TreePop();
						}
					}

					// --- Plane Shape Settings ---
					if (auto* ps = emitter->GetPlaneShape()) {
						if (ImGui::TreeNode("Plane Shape Settings")) {
							PlaneSettings s = ps->GetSettings();
							bool changed = false;

							changed |= ImGui::DragFloat2("Size", &s.size.x, 0.01f, 0.01f, 50.0f);
							
							int divX = static_cast<int>(s.divisionX);
							int divY = static_cast<int>(s.divisionY);
							if (ImGui::DragInt("Division X", &divX, 1, 1, 256)) {
								s.divisionX = static_cast<uint32_t>(divX);
								changed = true;
							}
							if (ImGui::DragInt("Division Y", &divY, 1, 1, 256)) {
								s.divisionY = static_cast<uint32_t>(divY);
								changed = true;
							}

							ImGui::Separator();
							changed |= ImGui::Checkbox("Flip V", &s.flipV);
							changed |= ImGui::Checkbox("Swap UV (U <-> V)", &s.isUvSwap);
							changed |= ImGui::ColorEdit4("Plane Color", &s.color.x);

							if (changed) {
								ps->SetSettings(s);
							}
							ImGui::TreePop();
						}
					}

					// ボタン類
					std::string emitLabel = "Emit " + groupName;
					if (ImGui::Button(emitLabel.c_str())) {
						ParticleManager::GetInstance()->Emit(groupName, emitter->GetTranslate(), emitter->GetCount());
					}
					std::string emitSelectedLabel = "Emit " + groupName + " from Selected Object";
					if (ImGui::Button(emitSelectedLabel.c_str())) {
						if (!object3ds_.empty()) {
							ParticleManager::GetInstance()->Emit(groupName, object3ds_[objectControlIndex_]->GetTranslate(), emitter->GetCount());
						}
					}
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
}

void DemoScene::UpdateImGui_Sprite() {
	ImGui::Separator();
	if (ImGui::TreeNode("Sprite")) {
		ImGui::Checkbox("showSprite", &isShowSprite_);

		for (size_t i = 0; i < sprites_.size(); ++i) {
			// ループ内のID衝突を防ぐ
			ImGui::PushID(static_cast<int>(i));

			std::string label = "Sprite " + std::to_string(i);
			if (ImGui::TreeNode(label.c_str())) {
				Sprite* sprite = sprites_[i].get();

				Vector2 spritePosition = sprite->GetTranslate();
				float spriteRotation = sprite->GetRotation();
				Vector2 spriteScale = sprite->GetScale();
				Vector4 spriteColor = sprite->GetColor();

				if (ImGui::SliderFloat2("translateSprite", &spritePosition.x, 0.0f, 1000.0f)) { sprite->SetTranslate(spritePosition); }
				if (ImGui::SliderFloat("rotateSprite", &spriteRotation, -6.28f, 6.28f)) { sprite->SetRotation(spriteRotation); }
				if (ImGui::SliderFloat2("scaleSprite", &spriteScale.x, 1.0f, Win32Window::kClientWidth)) { sprite->SetScale(spriteScale); }
				if (ImGui::ColorEdit4("colorSprite", &spriteColor.x)) { sprite->SetColor(spriteColor); }

				ImGui::Separator();
				ImGui::Text("Dissolve");
				bool enableDissolve = sprite->GetDissolveEnable() != 0;
				float dissolveThreshold = sprite->GetDissolveThreshold();
				float dissolveEdgeRange = sprite->GetDissolveEdgeRange();
				Vector3 dissolveEdgeColor = sprite->GetDissolveEdgeColor();
				std::string currentMask = sprite->GetDissolveMaskTexturePath();
				int maskIndex = (currentMask == "masks/noise1.png") ? 1 : 0;
				const char* maskPaths[] = { "masks/noise0.png", "masks/noise1.png" };
				const char* maskNames[] = { "noise0", "noise1" };

				bool dissolveChanged = false;
				if (ImGui::Checkbox("Enable Dissolve", &enableDissolve)) { dissolveChanged = true; }
				if (ImGui::SliderFloat("Threshold", &dissolveThreshold, 0.0f, 1.0f)) { dissolveChanged = true; }
				if (ImGui::SliderFloat("Edge Range", &dissolveEdgeRange, 0.0f, 0.2f)) { dissolveChanged = true; }
				float colorArr[3] = { dissolveEdgeColor.x, dissolveEdgeColor.y, dissolveEdgeColor.z };
				if (ImGui::ColorEdit3("Edge Color", colorArr)) {
					dissolveEdgeColor = { colorArr[0], colorArr[1], colorArr[2] };
					dissolveChanged = true;
				}
				if (ImGui::Combo("Mask Texture", &maskIndex, maskNames, 2)) {
					sprite->SetDissolveMaskTexture(maskPaths[maskIndex]);
				}
				if (dissolveChanged) {
					sprite->SetDissolveParams(enableDissolve ? 1 : 0, dissolveThreshold, dissolveEdgeRange, dissolveEdgeColor);
				}

				ImGui::Separator();
				ImGui::Text("UV");

				// UVTransform用の変数を用意
				Transform uvTransformSprite = {};
				uvTransformSprite.translate = sprite->GetUvTranslate();
				uvTransformSprite.scale = sprite->GetUvScale();
				uvTransformSprite.rotate = sprite->GetUvRotation();

				if (ImGui::DragFloat2("uvTranslate", &uvTransformSprite.translate.x, 0.01f, 0.0f, 1.0f)) { sprite->SetUvTranslate(uvTransformSprite.translate); }
				if (ImGui::DragFloat2("uvScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f)) { sprite->SetUvScale(uvTransformSprite.scale); }
				if (ImGui::SliderAngle("uvRotate", &uvTransformSprite.rotate.z)) { sprite->SetUvRotation(uvTransformSprite.rotate); }

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
}

void DemoScene::UpdateImGui_Sound() {
	// --- Sound ---
	ImGui::Separator();
	if (ImGui::TreeNode("Sound")) {
		if (ImGui::Button("Play Sound")) {
			// サウンドの再生
			AudioManager::GetInstance()->PlaySound("alarm1");
		}
		ImGui::TreePop();
	}
}

void DemoScene::UpdateImGui_Skybox() {
	// --- Skybox ---
	ImGui::Separator();
	if (ImGui::TreeNode("Skybox")) {
		ImGui::Checkbox("showSkybox", &isShowSkybox_);
		if (skybox_) {
			Vector3 scale = skybox_->GetScale();
			if (ImGui::DragFloat3("scale", &scale.x, 0.1f)) {
				skybox_->SetScale(scale);
			}
			Vector3 rotate = skybox_->GetRotate();
			if (ImGui::DragFloat3("rotate", &rotate.x, 0.01f)) {
				skybox_->SetRotate(rotate);
			}
			Vector3 translate = skybox_->GetTranslate();
			if (ImGui::DragFloat3("translate", &translate.x, 0.1f)) {
				skybox_->SetTranslate(translate);
			}
			Vector2 uvScale = skybox_->GetUvScale();
			if (ImGui::DragFloat2("UV Scale", &uvScale.x, 0.1f)) {
				skybox_->SetUvScale(uvScale);
			}
		}
		ImGui::TreePop();
	}
}

void DemoScene::UpdateImGui_HelpWindow() {
	ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(370.0f, 190.0f), ImGuiCond_Once);
	ImGui::Begin("Help");
	ImGui::Text("F1: Toggle Debug Camera");
	ImGui::Text("R: Reset Camera / Object Position");
	ImGui::Separator();
	ImGui::Text("Arrow Keys: Move Camera");
	ImGui::Separator();
	ImGui::Text("WASDQE: Move Object");
	ImGui::Text("ZC: Rotate Object");
	ImGui::Separator();
	ImGui::Text("SPACE: Emit Particles");
	ImGui::Separator();
	ImGui::Text("Use the combo box to select which object to control.");
	ImGui::Text("Adjust parameters in the 'Settings' window.");
	ImGui::End();
}

#endif // USE_IMGUI

void DemoScene::CreatePlayerSpawns(const std::vector<LevelData::PlayerSpawnData>& data) {
	(void)data;
}
