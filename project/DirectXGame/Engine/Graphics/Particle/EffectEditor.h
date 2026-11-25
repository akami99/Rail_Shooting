#pragma once

#include "BlendMode/BlendMode.h"
#include "ParticleEmitter.h"
#include <string>
#include <vector>

class Camera;

/// <summary>
/// パーティクルエフェクトの生成・調整・C++コード出力・JSON保存/読込を行うエディタツールクラス
/// </summary>
class EffectEditor {
public:
	EffectEditor() = default;
	~EffectEditor() = default;

	/// <summary>
	/// 初期化（テクスチャ候補や保存先ディレクトリのスキャンを行う）
	/// </summary>
	void Initialize(const std::string& effectsDir = "Resources/Effects");

	/// <summary>
	/// ImGuiによるエフェクトエディタUIの更新・描画
	/// </summary>
	void UpdateImGui(const Camera* camera = nullptr);

	/// <summary>
	/// エフェクトをJSONファイルに保存する
	/// </summary>
	bool SaveEffectToJson(const std::string& groupName, const std::string& filePath) const;

	/// <summary>
	/// JSONファイルからエフェクトを読み込み、ParticleManagerにグループ/エミッタを設定する
	/// </summary>
	bool LoadEffectFromJson(const std::string& groupName, const std::string& filePath);

	/// <summary>
	/// 現在のグループ設定をC++の初期化コード文字列として生成する
	/// </summary>
	std::string GenerateCppCode(const std::string& groupName, const std::string& varPrefix = "effect") const;

	// 状態・プレビュー設定の取得/設定
	bool IsWindowOpen() const { return isWindowOpen_; }
	void SetWindowOpen(bool open) { isWindowOpen_ = open; }
	bool IsUpdateParticle() const { return isUpdateParticle_; }
	void SetIsUpdateParticle(bool isUpdate) { isUpdateParticle_ = isUpdate; }
	bool UseBillboard() const { return useBillboard_; }
	void SetUseBillboard(bool useBillboard) { useBillboard_ = useBillboard; }
	BlendMode::BlendState GetBlendMode() const { return blendMode_; }
	void SetBlendMode(BlendMode::BlendState mode) { blendMode_ = mode; }

private:
	void RefreshTextureList();
	void RefreshJsonFileList();
	Vector3 CalculateCameraFrontPos(const Camera* camera, float distance) const;

#ifdef USE_IMGUI
	void DrawGroupSelectorSection();
	void DrawPreviewControlSection(const Camera* camera, const std::string& groupName, ParticleEmitter* emitter);
	void DrawEmitterSettingsSection(const std::string& groupName, ParticleEmitter* emitter);
	void DrawShapeSettingsSection(ParticleEmitter* emitter);
	void DrawCodeExportSection(const std::string& groupName);
	void DrawJsonSection(const std::string& groupName);
#endif // USE_IMGUI

private:
	std::string effectsDir_ = "Resources/Effects";
	std::vector<std::string> textureCandidates_;
	std::vector<std::string> jsonFiles_;

	int selectedGroupIndex_ = 0;
	std::string selectedGroupName_;

	// 新規グループ作成用バッファ
	char newGroupNameBuf_[128] = "NewEffectGroup";
	int newGroupTextureIndex_ = 0;

	// プレビュー・描画設定
	bool isWindowOpen_ = true;
	bool isUpdateParticle_ = true;
	bool useBillboard_ = false;
	BlendMode::BlendState blendMode_ = BlendMode::BlendState::kBlendModeAdd;
	float cameraEmitDistance_ = 5.0f;

	// コード出力用
	char codeVarPrefixBuf_[64] = "customEffect";
	std::string generatedCode_;
	bool autoRefreshCode_ = true;

	// JSON保存/読込用
	char jsonFileNameBuf_[128] = "custom_effect.json";
	int selectedJsonIndex_ = -1;
	std::string statusMessage_;
};
