#include "EffectEditor.h"
#include "Camera/Camera.h"
#include "ParticleManager.h"
#include "Texture/TextureManager.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif // USE_IMGUI

#include <externals/nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <unordered_map>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

json ToJson(const Vector2& v) { return json::array({ v.x, v.y }); }
json ToJson(const Vector3& v) { return json::array({ v.x, v.y, v.z }); }
json ToJson(const Vector4& v) { return json::array({ v.x, v.y, v.z, v.w }); }

std::string FmtVec2(const Vector2& v) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(3) << "{" << v.x << "f, " << v.y << "f}";
	return oss.str();
}

std::string FmtVec3(const Vector3& v) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(3) << "{" << v.x << "f, " << v.y << "f, " << v.z << "f}";
	return oss.str();
}

std::string FmtVec4(const Vector4& v) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(3) << "{" << v.x << "f, " << v.y << "f, " << v.z << "f, " << v.w << "f}";
	return oss.str();
}

std::string FmtFloat(float v) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(3) << v << "f";
	return oss.str();
}

std::string BoolStr(bool b) { return b ? "true" : "false"; }

class IShapeInspector {
public:
	virtual ~IShapeInspector() = default;
	virtual void ExportCppCode(std::ostringstream& oss, const std::string& emitterVar, const ParticleShape* shape) const = 0;
#ifdef USE_IMGUI
	virtual void DrawImGui(ParticleShape* shape) const = 0;
#endif
};

class BillboardInspector final : public IShapeInspector {
public:
	void ExportCppCode(std::ostringstream& oss, const std::string& emitterVar, const ParticleShape* shape) const override {
		(void)shape;
		oss << "  " << emitterVar << ".SetShapeType(ParticleShapeType::Billboard);\n";
	}
#ifdef USE_IMGUI
	void DrawImGui(ParticleShape* shape) const override { (void)shape; }
#endif
};

class RingInspector final : public IShapeInspector {
public:
	void ExportCppCode(std::ostringstream& oss, const std::string& emitterVar, const ParticleShape* shape) const override {
		oss << "  " << emitterVar << ".SetShapeType(ParticleShapeType::Ring);\n";
		if (const auto* rs = dynamic_cast<const RingShape*>(shape)) {
			const auto& s = rs->GetSettings();
			oss << "  if (auto *rs = " << emitterVar << ".GetRingShape()) {\n";
			oss << "    RingSettings s;\n";
			oss << "    s.innerRadius = " << FmtFloat(s.innerRadius) << ";\n";
			oss << "    s.startOuterRadius = " << FmtFloat(s.startOuterRadius) << ";\n";
			oss << "    s.midOuterRadius = " << FmtFloat(s.midOuterRadius) << ";\n";
			oss << "    s.endOuterRadius = " << FmtFloat(s.endOuterRadius) << ";\n";
			oss << "    s.startAngle = " << FmtFloat(s.startAngle) << ";\n";
			oss << "    s.endAngle = " << FmtFloat(s.endAngle) << ";\n";
			oss << "    s.division = " << s.division << ";\n";
			oss << "    s.isUvSwap = " << BoolStr(s.isUvSwap) << ";\n";
			oss << "    s.innerColor = " << FmtVec4(s.innerColor) << ";\n";
			oss << "    s.outerColor = " << FmtVec4(s.outerColor) << ";\n";
			oss << "    s.fadeStartAlpha = " << FmtFloat(s.fadeStartAlpha) << ";\n";
			oss << "    s.fadeEndAlpha = " << FmtFloat(s.fadeEndAlpha) << ";\n";
			oss << "    s.fadeRange = " << FmtFloat(s.fadeRange) << ";\n";
			oss << "    rs->SetSettings(s);\n";
			oss << "  }\n";
		}
	}
#ifdef USE_IMGUI
	void DrawImGui(ParticleShape* shape) const override {
		auto* rs = dynamic_cast<RingShape*>(shape);
		if (!rs) return;
		if (ImGui::TreeNode("Ring Shape Settings")) {
			RingSettings s = rs->GetSettings();
			bool changed = false;
			float tempInner = s.innerRadius;
			float tempOuter = s.startOuterRadius;
			if (ImGui::DragFloat("Inner Radius", &tempInner, 0.01f, 0.0f, 100.0f)) {
				if (tempOuter < tempInner) tempOuter = tempInner;
			}
			if (ImGui::DragFloat("Start Outer Radius", &tempOuter, 0.01f, tempInner, 100.0f)) {
				if (tempOuter < tempInner) tempOuter = tempInner;
			}
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
#endif
};

class CylinderInspector final : public IShapeInspector {
public:
	void ExportCppCode(std::ostringstream& oss, const std::string& emitterVar, const ParticleShape* shape) const override {
		oss << "  " << emitterVar << ".SetShapeType(ParticleShapeType::Cylinder);\n";
		if (const auto* cs = dynamic_cast<const CylinderShape*>(shape)) {
			const auto& s = cs->GetSettings();
			oss << "  if (auto *cs = " << emitterVar << ".GetCylinderShape()) {\n";
			oss << "    CylinderSettings s;\n";
			oss << "    s.height = " << FmtFloat(s.height) << ";\n";
			oss << "    s.topRadius = " << FmtVec2(s.topRadius) << ";\n";
			oss << "    s.bottomRadius = " << FmtVec2(s.bottomRadius) << ";\n";
			oss << "    s.startAngle = " << FmtFloat(s.startAngle) << ";\n";
			oss << "    s.endAngle = " << FmtFloat(s.endAngle) << ";\n";
			oss << "    s.division = " << s.division << ";\n";
			oss << "    s.verticalDivision = " << s.verticalDivision << ";\n";
			oss << "    s.flipV = " << BoolStr(s.flipV) << ";\n";
			oss << "    s.isUvSwap = " << BoolStr(s.isUvSwap) << ";\n";
			oss << "    s.topColor = " << FmtVec4(s.topColor) << ";\n";
			oss << "    s.bottomColor = " << FmtVec4(s.bottomColor) << ";\n";
			oss << "    s.fadeStartAlpha = " << FmtFloat(s.fadeStartAlpha) << ";\n";
			oss << "    s.fadeEndAlpha = " << FmtFloat(s.fadeEndAlpha) << ";\n";
			oss << "    s.fadeRange = " << FmtFloat(s.fadeRange) << ";\n";
			oss << "    s.alphaReference = " << FmtFloat(s.alphaReference) << ";\n";
			oss << "    cs->SetSettings(s);\n";
			oss << "  }\n";
		}
	}
#ifdef USE_IMGUI
	void DrawImGui(ParticleShape* shape) const override {
		auto* cs = dynamic_cast<CylinderShape*>(shape);
		if (!cs) return;
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
#endif
};

class PlaneInspector final : public IShapeInspector {
public:
	void ExportCppCode(std::ostringstream& oss, const std::string& emitterVar, const ParticleShape* shape) const override {
		oss << "  " << emitterVar << ".SetShapeType(ParticleShapeType::Plane);\n";
		if (const auto* ps = dynamic_cast<const PlaneShape*>(shape)) {
			const auto& s = ps->GetSettings();
			oss << "  if (auto *ps = " << emitterVar << ".GetPlaneShape()) {\n";
			oss << "    PlaneSettings s;\n";
			oss << "    s.size = " << FmtVec2(s.size) << ";\n";
			oss << "    s.divisionX = " << s.divisionX << ";\n";
			oss << "    s.divisionY = " << s.divisionY << ";\n";
			oss << "    s.flipV = " << BoolStr(s.flipV) << ";\n";
			oss << "    s.isUvSwap = " << BoolStr(s.isUvSwap) << ";\n";
			oss << "    s.color = " << FmtVec4(s.color) << ";\n";
			oss << "    ps->SetSettings(s);\n";
			oss << "  }\n";
		}
	}
#ifdef USE_IMGUI
	void DrawImGui(ParticleShape* shape) const override {
		auto* ps = dynamic_cast<PlaneShape*>(shape);
		if (!ps) return;
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
#endif
};

const IShapeInspector* GetShapeInspector(ParticleShapeType type) {
	static const std::unordered_map<ParticleShapeType, std::unique_ptr<IShapeInspector>> kInspectors = [] {
		std::unordered_map<ParticleShapeType, std::unique_ptr<IShapeInspector>> m;
		m.emplace(ParticleShapeType::Billboard, std::make_unique<BillboardInspector>());
		m.emplace(ParticleShapeType::Ring, std::make_unique<RingInspector>());
		m.emplace(ParticleShapeType::Cylinder, std::make_unique<CylinderInspector>());
		m.emplace(ParticleShapeType::Plane, std::make_unique<PlaneInspector>());
		return m;
	}();
	auto it = kInspectors.find(type);
	return (it != kInspectors.end()) ? it->second.get() : nullptr;
}

} // namespace

// =========================================================================
// Command Pattern (Undo / Redo) 実装
// =========================================================================
class EffectEditor::EditEmitterCommand final : public EffectEditor::IEffectCommand {
public:
	EditEmitterCommand(EffectEditor* editor, EditorSnapshot before, EditorSnapshot after, std::string label)
		: editor_(editor)
		, before_(std::move(before))
		, after_(std::move(after))
		, label_(std::move(label)) {
	}

	void Execute() override {
		if (editor_) {
			editor_->ApplySnapshot(after_);
		}
	}

	void Undo() override {
		if (editor_) {
			editor_->ApplySnapshot(before_);
		}
	}

	const char* GetLabel() const override { return label_.c_str(); }

private:
	EffectEditor* editor_ = nullptr;
	EditorSnapshot before_;
	EditorSnapshot after_;
	std::string label_;
};

EffectEditor::EditorSnapshot EffectEditor::CaptureSnapshot(const std::string& groupName, const ParticleEmitter& emitter) const {
	EditorSnapshot snap;
	snap.groupName = groupName;
	snap.emitter = emitter;
	snap.texturePath = ParticleManager::GetInstance()->GetGroupTexture(groupName);
	snap.blendMode = blendMode_;
	snap.useBillboard = useBillboard_;
	return snap;
}

void EffectEditor::ApplySnapshot(const EditorSnapshot& snapshot) {
	if (snapshot.groupName.empty()) return;
	if (ParticleManager::GetInstance()->GetEmitter(snapshot.groupName) == nullptr) return;

	if (!snapshot.texturePath.empty()) {
		TextureManager::GetInstance()->LoadTexture(snapshot.texturePath);
		ParticleManager::GetInstance()->SetGroupTexture(snapshot.groupName, snapshot.texturePath);
	}
	ParticleManager::GetInstance()->SetEmitter(snapshot.groupName, snapshot.emitter);
	blendMode_ = snapshot.blendMode;
	useBillboard_ = snapshot.useBillboard;
	selectedGroupName_ = snapshot.groupName;

	if (autoRefreshCode_) {
		generatedCode_ = GenerateCppCode(selectedGroupName_, codeVarPrefixBuf_);
	}
}

void EffectEditor::RecordCommand(std::unique_ptr<IEffectCommand> command) {
	if (!command) return;
	undoStack_.push_back(std::move(command));
	if (undoStack_.size() > kMaxHistorySize) {
		undoStack_.erase(undoStack_.begin());
	}
	redoStack_.clear();
}

void EffectEditor::Undo() {
	if (undoStack_.empty()) return;
	auto cmd = std::move(undoStack_.back());
	undoStack_.pop_back();
	cmd->Undo();
	redoStack_.push_back(std::move(cmd));
	hasPendingEdit_ = false;
}

void EffectEditor::Redo() {
	if (redoStack_.empty()) return;
	auto cmd = std::move(redoStack_.back());
	redoStack_.pop_back();
	cmd->Execute();
	undoStack_.push_back(std::move(cmd));
	hasPendingEdit_ = false;
}

void EffectEditor::ClearHistory() {
	undoStack_.clear();
	redoStack_.clear();
	hasPendingEdit_ = false;
}

void EffectEditor::Initialize(const std::string& effectsDir) {
	effectsDir_ = effectsDir;
	RefreshTextureList();
#ifdef USE_IMGUI
	// シーン初期化の一括同期・一時リソース解放前に候補テクスチャをプリロードしておく
	for (const auto& tex : textureCandidates_) {
		TextureManager::GetInstance()->LoadTexture(tex);
	}
#endif // USE_IMGUI
	RefreshJsonFileList();

	auto groupNames = ParticleManager::GetInstance()->GetParticleGroupNames();
	if (!groupNames.empty()) {
		selectedGroupIndex_ = 0;
		selectedGroupName_ = groupNames[0];
		generatedCode_ = GenerateCppCode(selectedGroupName_, codeVarPrefixBuf_);
	}
}

void EffectEditor::RefreshTextureList() {
	textureCandidates_.clear();

	const std::string particleTexDir = "Resources/Textures/Particles";
	if (fs::exists(particleTexDir) && fs::is_directory(particleTexDir)) {
		for (const auto& entry : fs::directory_iterator(particleTexDir)) {
			if (entry.is_regular_file()) {
				std::string ext = entry.path().extension().string();
				if (ext == ".png" || ext == ".jpg" || ext == ".dds") {
					textureCandidates_.push_back("Particles/" + entry.path().filename().string());
				}
			}
		}
	}

	// 基本テクスチャも候補に追加
	if (fs::exists("Resources/Textures/white.png")) {
		textureCandidates_.push_back("white.png");
	}
	if (textureCandidates_.empty()) {
		textureCandidates_.push_back("Particles/circle.png");
	}
}

void EffectEditor::RefreshJsonFileList() {
	jsonFiles_.clear();
	if (fs::exists(effectsDir_) && fs::is_directory(effectsDir_)) {
		for (const auto& entry : fs::directory_iterator(effectsDir_)) {
			if (entry.is_regular_file() && entry.path().extension() == ".json") {
				jsonFiles_.push_back(entry.path().filename().string());
			}
		}
	}
	std::sort(jsonFiles_.begin(), jsonFiles_.end());
}

Vector3 EffectEditor::CalculateCameraFrontPos(const Camera* camera, float distance) const {
	if (!camera) {
		return { 0.0f, 1.0f, distance };
	}
	const Matrix4x4& world = camera->GetWorldMatrix();
	Vector3 forward = { world.m[2][0], world.m[2][1], world.m[2][2] };
	float len = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (len > 0.0001f) {
		forward.x /= len;
		forward.y /= len;
		forward.z /= len;
	} else {
		forward = { 0.0f, 0.0f, 1.0f };
	}
	const Vector3& camPos = camera->GetTranslate();
	return {
		camPos.x + forward.x * distance,
		camPos.y + forward.y * distance,
		camPos.z + forward.z * distance
	};
}

bool EffectEditor::SaveEffectToJson(const std::string& groupName, const std::string& filePath) const {
	const ParticleEmitter* emitter = ParticleManager::GetInstance()->GetEmitter(groupName);
	if (!emitter) {
		return false;
	}

	json root;
	root["groupName"] = groupName;
	root["texture"] = ParticleManager::GetInstance()->GetGroupTexture(groupName);
	root["blendMode"] = static_cast<int>(blendMode_);
	root["useBillboard"] = useBillboard_;

	const Transform& emitterTransform = emitter->GetTransform();
	root["emitter"] = {
		{ "transform", {
			{ "scale", ToJson(emitterTransform.scale) },
			{ "rotate", ToJson(emitterTransform.rotate) },
			{ "translate", ToJson(emitterTransform.translate) }
		}},
		{ "count", emitter->GetCount() },
		{ "frequency", emitter->GetFrequency() },
		{ "isEmit", emitter->IsEmit() },
		{ "isEffectMode", emitter->IsEffectMode() },
		{ "isLoop", emitter->IsLoop() }
	};

	const auto& gs = emitter->GetGenerateSettings();
	root["generateSettings"] = {
		{ "isRandomScale", gs.isRandomScale },
		{ "scaleMin", ToJson(gs.scaleMin) },
		{ "scaleMax", ToJson(gs.scaleMax) },
		{ "fixedScale", ToJson(gs.fixedScale) },
		{ "isRandomRotate", gs.isRandomRotate },
		{ "rotateMin", ToJson(gs.rotateMin) },
		{ "rotateMax", ToJson(gs.rotateMax) },
		{ "fixedRotate", ToJson(gs.fixedRotate) },
		{ "isRandomVelocity", gs.isRandomVelocity },
		{ "velocityMin", ToJson(gs.velocityMin) },
		{ "velocityMax", ToJson(gs.velocityMax) },
		{ "fixedVelocity", ToJson(gs.fixedVelocity) },
		{ "isRandomLifeTime", gs.isRandomLifeTime },
		{ "lifeTimeMin", gs.lifeTimeMin },
		{ "lifeTimeMax", gs.lifeTimeMax },
		{ "fixedLifeTime", gs.fixedLifeTime },
		{ "isRandomColor", gs.isRandomColor },
		{ "colorMin", ToJson(gs.colorMin) },
		{ "colorMax", ToJson(gs.colorMax) },
		{ "fixedColor", ToJson(gs.fixedColor) }
	};

	const auto& fsSettings = emitter->GetFieldSettings();
	root["fieldSettings"] = {
		{ "isAccelerationFieldActive", fsSettings.isAccelerationFieldActive },
		{ "acceleration", ToJson(fsSettings.accelerationField.acceleration) },
		{ "areaMin", ToJson(fsSettings.accelerationField.area.min) },
		{ "areaMax", ToJson(fsSettings.accelerationField.area.max) },
		{ "isGravityFieldActive", fsSettings.isGravityFieldActive },
		{ "gravity", ToJson(fsSettings.gravity) }
	};

	const auto& uvas = emitter->GetUVAnimationSettings();
	root["uvAnimationSettings"] = {
		{ "isActive", uvas.isActive },
		{ "isIndividual", uvas.isIndividual },
		{ "scrollSpeed", ToJson(uvas.scrollSpeed) },
		{ "rotateSpeed", uvas.rotateSpeed },
		{ "scaleSpeed", ToJson(uvas.scaleSpeed) }
	};

	root["shapeType"] = static_cast<int>(emitter->GetShapeType());
	if (const auto* shape = emitter->GetShape()) {
		shape->SaveToJson(root);
	}

	fs::path path(filePath);
	if (path.has_parent_path()) {
		fs::create_directories(path.parent_path());
	}

	std::ofstream ofs(filePath);
	if (!ofs.is_open()) {
		return false;
	}
	ofs << root.dump(2);
	return true;
}

bool EffectEditor::LoadEffectFromJson(const std::string& groupName, const std::string& filePath) {
	return ParticleManager::GetInstance()->LoadEffectFromJson(groupName, filePath, &blendMode_, &useBillboard_);
}

std::string EffectEditor::GenerateCppCode(const std::string& groupName, const std::string& varPrefix) const {
	const ParticleEmitter* emitter = ParticleManager::GetInstance()->GetEmitter(groupName);
	if (!emitter) {
		return "// Emitter not found: " + groupName;
	}

	std::string texPath = ParticleManager::GetInstance()->GetGroupTexture(groupName);
	std::string prefix = varPrefix.empty() ? "effect" : varPrefix;
	std::string emitterVar = prefix + "Emitter";
	std::string transformVar = prefix + "Transform";

	// C++コード生成
	const Transform& emitterTransform = emitter->GetTransform();
	std::ostringstream oss;
	oss << "// --- " << groupName << " ---\n";
	oss << "TextureManager::GetInstance()->LoadTexture(\"" << texPath << "\");\n";
	oss << "ParticleManager::GetInstance()->CreateParticleGroup(\"" << groupName << "\", \"" << texPath << "\");\n";
	oss << "{\n";
	oss << "  Transform " << transformVar << " = {\n";
	oss << "      " << FmtVec3(emitterTransform.scale) << ", // scale\n";
	oss << "      " << FmtVec3(emitterTransform.rotate) << ", // rotate\n";
	oss << "      " << FmtVec3(emitterTransform.translate) << " // translate\n";
	oss << "  };\n";
	oss << "  ParticleEmitter " << emitterVar << "(" << transformVar << ", " << emitter->GetCount() << ", " << FmtFloat(emitter->GetFrequency()) << ");\n";
	oss << "  " << emitterVar << ".SetIsEmit(" << BoolStr(emitter->IsEmit()) << ");\n";
	oss << "  " << emitterVar << ".SetIsEffectMode(" << BoolStr(emitter->IsEffectMode()) << ");\n";
	oss << "  " << emitterVar << ".SetIsLoop(" << BoolStr(emitter->IsLoop()) << ");\n\n";

	const auto& gs = emitter->GetGenerateSettings();
	oss << "  ParticleGenerateSettings gs;\n";
	oss << "  gs.isRandomScale = " << BoolStr(gs.isRandomScale) << ";\n";
	if (gs.isRandomScale) {
		oss << "  gs.scaleMin = " << FmtVec3(gs.scaleMin) << ";\n";
		oss << "  gs.scaleMax = " << FmtVec3(gs.scaleMax) << ";\n";
	} else {
		oss << "  gs.fixedScale = " << FmtVec3(gs.fixedScale) << ";\n";
	}

	oss << "  gs.isRandomRotate = " << BoolStr(gs.isRandomRotate) << ";\n";
	if (gs.isRandomRotate) {
		oss << "  gs.rotateMin = " << FmtVec3(gs.rotateMin) << ";\n";
		oss << "  gs.rotateMax = " << FmtVec3(gs.rotateMax) << ";\n";
	} else {
		oss << "  gs.fixedRotate = " << FmtVec3(gs.fixedRotate) << ";\n";
	}

	oss << "  gs.isRandomVelocity = " << BoolStr(gs.isRandomVelocity) << ";\n";
	if (gs.isRandomVelocity) {
		oss << "  gs.velocityMin = " << FmtVec3(gs.velocityMin) << ";\n";
		oss << "  gs.velocityMax = " << FmtVec3(gs.velocityMax) << ";\n";
	} else {
		oss << "  gs.fixedVelocity = " << FmtVec3(gs.fixedVelocity) << ";\n";
	}

	oss << "  gs.isRandomLifeTime = " << BoolStr(gs.isRandomLifeTime) << ";\n";
	if (gs.isRandomLifeTime) {
		oss << "  gs.lifeTimeMin = " << FmtFloat(gs.lifeTimeMin) << ";\n";
		oss << "  gs.lifeTimeMax = " << FmtFloat(gs.lifeTimeMax) << ";\n";
	} else {
		oss << "  gs.fixedLifeTime = " << FmtFloat(gs.fixedLifeTime) << ";\n";
	}

	oss << "  gs.isRandomColor = " << BoolStr(gs.isRandomColor) << ";\n";
	if (gs.isRandomColor) {
		oss << "  gs.colorMin = " << FmtVec4(gs.colorMin) << ";\n";
		oss << "  gs.colorMax = " << FmtVec4(gs.colorMax) << ";\n";
	} else {
		oss << "  gs.fixedColor = " << FmtVec4(gs.fixedColor) << ";\n";
	}
	oss << "  " << emitterVar << ".SetGenerateSettings(gs);\n\n";

	const auto& fsSettings = emitter->GetFieldSettings();
	oss << "  ParticleFieldSettings fs;\n";
	oss << "  fs.isAccelerationFieldActive = " << BoolStr(fsSettings.isAccelerationFieldActive) << ";\n";
	if (fsSettings.isAccelerationFieldActive) {
		oss << "  fs.accelerationField.acceleration = " << FmtVec3(fsSettings.accelerationField.acceleration) << ";\n";
		oss << "  fs.accelerationField.area = {" << FmtVec3(fsSettings.accelerationField.area.min) << ", " << FmtVec3(fsSettings.accelerationField.area.max) << "};\n";
	}
	oss << "  fs.isGravityFieldActive = " << BoolStr(fsSettings.isGravityFieldActive) << ";\n";
	if (fsSettings.isGravityFieldActive) {
		oss << "  fs.gravity = " << FmtVec3(fsSettings.gravity) << ";\n";
	}
	oss << "  " << emitterVar << ".SetFieldSettings(fs);\n\n";

	const auto& uvas = emitter->GetUVAnimationSettings();
	oss << "  ParticleUVAnimationSettings uvas;\n";
	oss << "  uvas.isActive = " << BoolStr(uvas.isActive) << ";\n";
	if (uvas.isActive) {
		oss << "  uvas.isIndividual = " << BoolStr(uvas.isIndividual) << ";\n";
		oss << "  uvas.scrollSpeed = " << FmtVec2(uvas.scrollSpeed) << ";\n";
		oss << "  uvas.rotateSpeed = " << FmtFloat(uvas.rotateSpeed) << ";\n";
		oss << "  uvas.scaleSpeed = " << FmtVec2(uvas.scaleSpeed) << ";\n";
	}
	oss << "  " << emitterVar << ".SetUVAnimationSettings(uvas);\n\n";

	if (const auto* inspector = GetShapeInspector(emitter->GetShapeType())) {
		inspector->ExportCppCode(oss, emitterVar, emitter->GetShape());
	}

	oss << "  ParticleManager::GetInstance()->SetEmitter(\"" << groupName << "\", " << emitterVar << ");\n";
	oss << "}\n";
	return oss.str();
}

void EffectEditor::UpdateImGui(const Camera* camera) {
#ifdef USE_IMGUI
	if (!isWindowOpen_) {
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(480.0f, 680.0f), ImGuiCond_Once);
	if (ImGui::Begin("Effect Editor", &isWindowOpen_)) {
		// キーボードショートカット (Ctrl+Z / Ctrl+Y)
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
			const ImGuiIO& io = ImGui::GetIO();
			if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
				Undo();
			} else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
				Redo();
			}
		}

		// Undo / Redo ツールバー
		ImGui::BeginDisabled(!CanUndo());
		if (ImGui::Button("Undo (Ctrl+Z)")) {
			Undo();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!CanRedo());
		if (ImGui::Button("Redo (Ctrl+Y)")) {
			Redo();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("History: %zu / %zu", undoStack_.size(), redoStack_.size());
		ImGui::Separator();

		std::string prevGroup = selectedGroupName_;
		DrawGroupSelectorSection();
		if (prevGroup != selectedGroupName_) {
			hasPendingEdit_ = false;
		}

		ParticleEmitter* emitter = selectedGroupName_.empty()
			? nullptr
			: ParticleManager::GetInstance()->GetEmitter(selectedGroupName_);

		if (emitter) {
			EditorSnapshot beforeFrameSnapshot;
			if (!hasPendingEdit_) {
				beforeFrameSnapshot = CaptureSnapshot(selectedGroupName_, *emitter);
			}

			ImGui::Separator();
			bool edited = false;
			edited |= DrawPreviewControlSection(camera, selectedGroupName_, emitter);
			ImGui::Separator();
			edited |= DrawEmitterSettingsSection(selectedGroupName_, emitter);
			edited |= DrawShapeSettingsSection(emitter);

			if (!hasPendingEdit_ && edited) {
				pendingBeforeSnapshot_ = std::move(beforeFrameSnapshot);
				hasPendingEdit_ = true;
			}
			if (hasPendingEdit_ && !ImGui::IsAnyItemActive()) {
				EditorSnapshot afterSnapshot = CaptureSnapshot(selectedGroupName_, *emitter);
				RecordCommand(std::make_unique<EditEmitterCommand>(
					this, std::move(pendingBeforeSnapshot_), std::move(afterSnapshot), "Edit Effect"));
				hasPendingEdit_ = false;
			}

			ImGui::Separator();
			DrawCodeExportSection(selectedGroupName_);
			ImGui::Separator();
			DrawJsonSection(selectedGroupName_);
		} else {
			hasPendingEdit_ = false;
			ImGui::TextDisabled("編集可能なパーティクルグループがありません。");
		}
	}
	ImGui::End();
#else
	(void)camera;
#endif // USE_IMGUI
}

#ifdef USE_IMGUI

void EffectEditor::DrawGroupSelectorSection() {
	std::vector<std::string> groupNames = ParticleManager::GetInstance()->GetParticleGroupNames();

	if (!groupNames.empty()) {
		if (selectedGroupName_.empty()) {
			selectedGroupIndex_ = 0;
			selectedGroupName_ = groupNames[0];
		} else {
			auto it = std::find(groupNames.begin(), groupNames.end(), selectedGroupName_);
			if (it != groupNames.end()) {
				selectedGroupIndex_ = static_cast<int>(std::distance(groupNames.begin(), it));
			} else {
				selectedGroupIndex_ = 0;
				selectedGroupName_ = groupNames[0];
			}
		}

		if (ImGui::BeginCombo("Target Group", selectedGroupName_.c_str())) {
			for (int i = 0; i < static_cast<int>(groupNames.size()); ++i) {
				bool isSelected = (selectedGroupIndex_ == i);
				if (ImGui::Selectable(groupNames[i].c_str(), isSelected)) {
					selectedGroupIndex_ = i;
					selectedGroupName_ = groupNames[i];
					if (autoRefreshCode_) {
						generatedCode_ = GenerateCppCode(selectedGroupName_, codeVarPrefixBuf_);
					}
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}

	if (ImGui::TreeNode("Create New Effect Group")) {
		ImGui::InputText("Group Name", newGroupNameBuf_, sizeof(newGroupNameBuf_));

		if (!textureCandidates_.empty()) {
			newGroupTextureIndex_ = std::clamp(newGroupTextureIndex_, 0, static_cast<int>(textureCandidates_.size()) - 1);
			if (ImGui::BeginCombo("Initial Texture", textureCandidates_[newGroupTextureIndex_].c_str())) {
				for (int i = 0; i < static_cast<int>(textureCandidates_.size()); ++i) {
					bool isSelected = (newGroupTextureIndex_ == i);
					if (ImGui::Selectable(textureCandidates_[i].c_str(), isSelected)) {
						newGroupTextureIndex_ = i;
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}

		if (ImGui::Button("Create Group")) {
			std::string newName(newGroupNameBuf_);
			if (!newName.empty()) {
				std::string texPath = textureCandidates_.empty() ? "Particles/circle.png" : textureCandidates_[newGroupTextureIndex_];
				TextureManager::GetInstance()->LoadTexture(texPath);
				if (ParticleManager::GetInstance()->GetEmitter(newName) == nullptr) {
					ParticleManager::GetInstance()->CreateParticleGroup(newName, texPath);
					ParticleEmitter defaultEmitter;
					defaultEmitter.SetIsEmit(false);
					defaultEmitter.SetIsEffectMode(true);
					ParticleManager::GetInstance()->SetEmitter(newName, defaultEmitter);
				}
				selectedGroupName_ = newName;
				if (autoRefreshCode_) {
					generatedCode_ = GenerateCppCode(selectedGroupName_, codeVarPrefixBuf_);
				}
				statusMessage_ = "Created group: " + newName;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Refresh Textures")) {
			RefreshTextureList();
		}
		ImGui::TreePop();
	}
}

bool EffectEditor::DrawPreviewControlSection(const Camera* camera, const std::string& groupName, ParticleEmitter* emitter) {
	bool edited = false;
	if (ImGui::CollapsingHeader("Preview & Play Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Update Particles", &isUpdateParticle_);
		ImGui::SameLine();
		edited |= ImGui::Checkbox("Use Billboard", &useBillboard_);

		const char* blendNames[] = { "None", "Normal", "Add", "Subtract", "Multiply", "Screen" };
		int blendIdx = static_cast<int>(blendMode_);
		if (ImGui::Combo("Blend Mode", &blendIdx, blendNames, 6)) {
			blendMode_ = static_cast<BlendMode::BlendState>(blendIdx);
			edited = true;
		}

		ImGui::DragFloat("Camera Front Dist", &cameraEmitDistance_, 0.1f, 0.5f, 50.0f);

		if (ImGui::Button("Emit at Emitter Pos")) {
			ParticleManager::GetInstance()->Emit(groupName, emitter->GetTranslate(), emitter->GetCount());
		}
		ImGui::SameLine();
		if (ImGui::Button("Emit in Front of Camera")) {
			Vector3 pos = CalculateCameraFrontPos(camera, cameraEmitDistance_);
			ParticleManager::GetInstance()->Emit(groupName, pos, emitter->GetCount());
		}

		if (ImGui::Button("Move Emitter to Camera Front")) {
			emitter->SetTranslate(CalculateCameraFrontPos(camera, cameraEmitDistance_));
			edited = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Group")) {
			ParticleManager::GetInstance()->ClearParticles(groupName);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear All")) {
			ParticleManager::GetInstance()->ClearAllParticles();
		}
	}
	return edited;
}

bool EffectEditor::DrawEmitterSettingsSection(const std::string& groupName, ParticleEmitter* emitter) {
	bool sectionEdited = false;
	auto DragFloatMinMax = [&](const char* labelMin, const char* labelMax, float* minVal, float* maxVal, float speed = 0.01f) {
		if (ImGui::DragFloat(labelMin, minVal, speed, 0.0f, *maxVal)) {
			if (*minVal > *maxVal) *minVal = *maxVal;
			sectionEdited = true;
		}
		if (ImGui::DragFloat(labelMax, maxVal, speed, *minVal, 100.0f)) {
			if (*maxVal < *minVal) *maxVal = *minVal;
			sectionEdited = true;
		}
	};
	auto DragFloat3MinMax = [&](const char* labelMin, const char* labelMax, Vector3* minVal, Vector3* maxVal, float speed = 0.01f) {
		sectionEdited |= ImGui::DragFloat3(labelMin, &minVal->x, speed);
		sectionEdited |= ImGui::DragFloat3(labelMax, &maxVal->x, speed);
		if (minVal->x > maxVal->x) minVal->x = maxVal->x;
		if (minVal->y > maxVal->y) minVal->y = maxVal->y;
		if (minVal->z > maxVal->z) minVal->z = maxVal->z;
	};

	if (ImGui::CollapsingHeader("Emitter & Animation Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		Vector3 translate = emitter->GetTranslate();
		if (ImGui::DragFloat3("Translate", &translate.x, 0.01f)) { emitter->SetTranslate(translate); sectionEdited = true; }
		Vector3 rotate = emitter->GetRotate();
		if (ImGui::DragFloat3("Rotate", &rotate.x, 0.01f)) { emitter->SetRotate(rotate); sectionEdited = true; }
		Vector3 scale = emitter->GetScale();
		if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) { emitter->SetScale(scale); sectionEdited = true; }

		int count = static_cast<int>(emitter->GetCount());
		if (ImGui::DragInt("Count", &count, 1, 1, 100)) {
			emitter->SetCount(static_cast<uint32_t>(count));
			sectionEdited = true;
		}

		// テクスチャ選択
		std::string currentTex = ParticleManager::GetInstance()->GetGroupTexture(groupName);
		if (ImGui::BeginCombo("Texture", currentTex.c_str())) {
			for (const auto& tex : textureCandidates_) {
				bool isSelected = (currentTex == tex);
				if (ImGui::Selectable(tex.c_str(), isSelected)) {
					ParticleManager::GetInstance()->SetGroupTexture(groupName, tex);
					sectionEdited = true;
					if (autoRefreshCode_) {
						generatedCode_ = GenerateCppCode(groupName, codeVarPrefixBuf_);
					}
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		bool isEmit = emitter->IsEmit();
		if (ImGui::Checkbox("Enable Auto Emit", &isEmit)) { emitter->SetIsEmit(isEmit); sectionEdited = true; }
		float frequency = emitter->GetFrequency();
		if (ImGui::DragFloat("Frequency", &frequency, 0.01f, 0.01f, 10.0f)) { emitter->SetFrequency(frequency); sectionEdited = true; }

		bool isEffectMode = emitter->IsEffectMode();
		if (ImGui::Checkbox("Effect Mode (Single/Loop)", &isEffectMode)) { emitter->SetIsEffectMode(isEffectMode); sectionEdited = true; }
		if (emitter->IsEffectMode()) {
			bool isLoop = emitter->IsLoop();
			if (ImGui::Checkbox("Loop Effect", &isLoop)) { emitter->SetIsLoop(isLoop); sectionEdited = true; }
			ImGui::Text("Playing State: %s", emitter->IsPlaying() ? "Playing" : "Stopped");
		}

		// Generate Settings
		if (ImGui::TreeNode("Generate Settings")) {
			ParticleGenerateSettings gs = emitter->GetGenerateSettings();
			bool changed = false;
			changed |= ImGui::Checkbox("Random Scale", &gs.isRandomScale);
			if (gs.isRandomScale) {
				DragFloat3MinMax("Scale Min", "Scale Max", &gs.scaleMin, &gs.scaleMax);
				changed = true;
			} else {
				changed |= ImGui::DragFloat3("Fixed Scale", &gs.fixedScale.x, 0.01f);
			}

			changed |= ImGui::Checkbox("Random Rotate", &gs.isRandomRotate);
			if (gs.isRandomRotate) {
				DragFloat3MinMax("Rotate Min", "Rotate Max", &gs.rotateMin, &gs.rotateMax);
				changed = true;
			} else {
				changed |= ImGui::DragFloat3("Fixed Rotate", &gs.fixedRotate.x, 0.01f);
			}

			changed |= ImGui::Checkbox("Random Velocity", &gs.isRandomVelocity);
			if (gs.isRandomVelocity) {
				DragFloat3MinMax("Velocity Min", "Velocity Max", &gs.velocityMin, &gs.velocityMax);
				changed = true;
			} else {
				changed |= ImGui::DragFloat3("Fixed Velocity", &gs.fixedVelocity.x, 0.01f);
			}

			changed |= ImGui::Checkbox("Random LifeTime", &gs.isRandomLifeTime);
			if (gs.isRandomLifeTime) {
				DragFloatMinMax("LifeTime Min", "LifeTime Max", &gs.lifeTimeMin, &gs.lifeTimeMax);
				changed = true;
			} else {
				changed |= ImGui::DragFloat("Fixed LifeTime", &gs.fixedLifeTime, 0.01f, 0.01f, 60.0f);
			}

			changed |= ImGui::Checkbox("Random Color", &gs.isRandomColor);
			if (gs.isRandomColor) {
				changed |= ImGui::ColorEdit4("Color Min", &gs.colorMin.x);
				changed |= ImGui::ColorEdit4("Color Max", &gs.colorMax.x);
			} else {
				changed |= ImGui::ColorEdit4("Fixed Color", &gs.fixedColor.x);
			}
			if (changed) {
				emitter->SetGenerateSettings(gs);
				sectionEdited = true;
			}
			ImGui::TreePop();
		}

		// Field Settings
		if (ImGui::TreeNode("Field Settings")) {
			ParticleFieldSettings fsSettings = emitter->GetFieldSettings();
			bool changed = false;
			changed |= ImGui::Checkbox("Acceleration Field Active", &fsSettings.isAccelerationFieldActive);
			if (fsSettings.isAccelerationFieldActive) {
				changed |= ImGui::DragFloat3("Acceleration", &fsSettings.accelerationField.acceleration.x, 0.1f);
				DragFloat3MinMax("Area Min", "Area Max", &fsSettings.accelerationField.area.min, &fsSettings.accelerationField.area.max, 0.1f);
				changed = true;
			}
			ImGui::Separator();
			changed |= ImGui::Checkbox("Gravity Field Active", &fsSettings.isGravityFieldActive);
			if (fsSettings.isGravityFieldActive) {
				changed |= ImGui::DragFloat3("Gravity Vector", &fsSettings.gravity.x, 0.1f);
			}
			if (changed) {
				emitter->SetFieldSettings(fsSettings);
				sectionEdited = true;
			}
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
					uvas.currentTranslate = { 0.0f, 0.0f };
					uvas.currentRotate = 0.0f;
					uvas.currentScale = { 1.0f, 1.0f };
					changed = true;
				}
			}
			if (changed) {
				emitter->SetUVAnimationSettings(uvas);
				sectionEdited = true;
			}
			ImGui::TreePop();
		}
	}
	return sectionEdited;
}

bool EffectEditor::DrawShapeSettingsSection(ParticleEmitter* emitter) {
	bool edited = false;
	if (ImGui::CollapsingHeader("Shape Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* shapeItems[] = { "Billboard", "Ring", "Cylinder", "Plane" };
		int shapeIndex = static_cast<int>(emitter->GetShapeType());
		if (ImGui::Combo("Shape Type", &shapeIndex, shapeItems, 4)) {
			emitter->SetShapeType(static_cast<ParticleShapeType>(shapeIndex));
			edited = true;
		}

		if (const auto* inspector = GetShapeInspector(emitter->GetShapeType())) {
			inspector->DrawImGui(emitter->GetShape());
			if (ImGui::IsAnyItemActive()) {
				edited = true;
			}
		}
	}
	return edited;
}

void EffectEditor::DrawCodeExportSection(const std::string& groupName) {
	if (ImGui::CollapsingHeader("C++ Code Export (Copy & Paste)")) {
		ImGui::InputText("Var Prefix", codeVarPrefixBuf_, sizeof(codeVarPrefixBuf_));
		ImGui::Checkbox("Auto Refresh", &autoRefreshCode_);
		ImGui::SameLine();
		if (ImGui::Button("Generate Code") || autoRefreshCode_) {
			generatedCode_ = GenerateCppCode(groupName, codeVarPrefixBuf_);
		}
		ImGui::SameLine();
		if (ImGui::Button("Copy to Clipboard")) {
			generatedCode_ = GenerateCppCode(groupName, codeVarPrefixBuf_);
			ImGui::SetClipboardText(generatedCode_.c_str());
			statusMessage_ = "Copied C++ code to clipboard!";
		}

		ImGui::InputTextMultiline(
			"##GeneratedCppCode",
			const_cast<char*>(generatedCode_.c_str()),
			generatedCode_.size() + 1,
			ImVec2(-FLT_MIN, 180.0f),
			ImGuiInputTextFlags_ReadOnly
		);
	}
}

void EffectEditor::DrawJsonSection(const std::string& groupName) {
	if (ImGui::CollapsingHeader("JSON Save / Load")) {
		ImGui::InputText("File Name", jsonFileNameBuf_, sizeof(jsonFileNameBuf_));

		if (ImGui::Button("Save JSON")) {
			std::string fileName(jsonFileNameBuf_);
			if (!fileName.empty()) {
				if (fs::path(fileName).extension() != ".json") {
					fileName += ".json";
				}
				std::string fullPath = (fs::path(effectsDir_) / fileName).string();
				if (SaveEffectToJson(groupName, fullPath)) {
					statusMessage_ = "Saved: " + fullPath;
					RefreshJsonFileList();
				} else {
					statusMessage_ = "Failed to save: " + fullPath;
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Refresh List")) {
			RefreshJsonFileList();
		}

		if (!jsonFiles_.empty()) {
			const char* preview = (selectedJsonIndex_ >= 0 && selectedJsonIndex_ < static_cast<int>(jsonFiles_.size()))
				? jsonFiles_[selectedJsonIndex_].c_str()
				: "Select JSON...";
			if (ImGui::BeginCombo("Saved Effects", preview)) {
				for (int i = 0; i < static_cast<int>(jsonFiles_.size()); ++i) {
					bool isSelected = (selectedJsonIndex_ == i);
					if (ImGui::Selectable(jsonFiles_[i].c_str(), isSelected)) {
						selectedJsonIndex_ = i;
						std::snprintf(jsonFileNameBuf_, sizeof(jsonFileNameBuf_), "%s", jsonFiles_[i].c_str());
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (selectedJsonIndex_ >= 0 && selectedJsonIndex_ < static_cast<int>(jsonFiles_.size())) {
				if (ImGui::Button("Load Selected into Current Group")) {
					std::string fullPath = (fs::path(effectsDir_) / jsonFiles_[selectedJsonIndex_]).string();
					if (LoadEffectFromJson(groupName, fullPath)) {
						statusMessage_ = "Loaded into " + groupName;
						if (autoRefreshCode_) {
							generatedCode_ = GenerateCppCode(groupName, codeVarPrefixBuf_);
						}
					} else {
						statusMessage_ = "Failed to load: " + fullPath;
					}
				}
			}
		}

		if (!statusMessage_.empty()) {
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "%s", statusMessage_.c_str());
		}
	}
}

#endif // USE_IMGUI
