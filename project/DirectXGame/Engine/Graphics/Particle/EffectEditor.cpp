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
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

json ToJson(const Vector2& v) { return json::array({ v.x, v.y }); }
json ToJson(const Vector3& v) { return json::array({ v.x, v.y, v.z }); }
json ToJson(const Vector4& v) { return json::array({ v.x, v.y, v.z, v.w }); }

Vector2 FromJsonVec2(const json& j, const Vector2& fallback = { 0.0f, 0.0f }) {
	if (j.is_array() && j.size() >= 2) {
		return { j[0].get<float>(), j[1].get<float>() };
	}
	return fallback;
}

Vector3 FromJsonVec3(const json& j, const Vector3& fallback = { 0.0f, 0.0f, 0.0f }) {
	if (j.is_array() && j.size() >= 3) {
		return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>() };
	}
	return fallback;
}

Vector4 FromJsonVec4(const json& j, const Vector4& fallback = { 1.0f, 1.0f, 1.0f, 1.0f }) {
	if (j.is_array() && j.size() >= 4) {
		return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>() };
	}
	return fallback;
}

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

} // namespace

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
	ParticleEmitter* emitter = ParticleManager::GetInstance()->GetEmitter(groupName);
	if (!emitter) {
		return false;
	}

	json root;
	root["groupName"] = groupName;
	root["texture"] = ParticleManager::GetInstance()->GetGroupTexture(groupName);
	root["blendMode"] = static_cast<int>(blendMode_);
	root["useBillboard"] = useBillboard_;

	root["emitter"] = {
		{ "transform", {
			{ "scale", ToJson(emitter->transform.scale) },
			{ "rotate", ToJson(emitter->transform.rotate) },
			{ "translate", ToJson(emitter->transform.translate) }
		}},
		{ "count", emitter->count },
		{ "frequency", emitter->frequency },
		{ "isEmit", emitter->isEmit },
		{ "isEffectMode", emitter->isEffectMode },
		{ "isLoop", emitter->isLoop }
	};

	const auto& gs = emitter->generateSettings;
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

	const auto& fsSettings = emitter->fieldSettings;
	root["fieldSettings"] = {
		{ "isAccelerationFieldActive", fsSettings.isAccelerationFieldActive },
		{ "acceleration", ToJson(fsSettings.accelerationField.acceleration) },
		{ "areaMin", ToJson(fsSettings.accelerationField.area.min) },
		{ "areaMax", ToJson(fsSettings.accelerationField.area.max) },
		{ "isGravityFieldActive", fsSettings.isGravityFieldActive },
		{ "gravity", ToJson(fsSettings.gravity) }
	};

	const auto& uvas = emitter->uvAnimationSettings;
	root["uvAnimationSettings"] = {
		{ "isActive", uvas.isActive },
		{ "isIndividual", uvas.isIndividual },
		{ "scrollSpeed", ToJson(uvas.scrollSpeed) },
		{ "rotateSpeed", uvas.rotateSpeed },
		{ "scaleSpeed", ToJson(uvas.scaleSpeed) }
	};

	root["shapeType"] = static_cast<int>(emitter->GetShapeType());
	if (auto* rs = emitter->GetRingShape()) {
		const auto& s = rs->settings;
		root["ringSettings"] = {
			{ "innerRadius", s.innerRadius },
			{ "startOuterRadius", s.startOuterRadius },
			{ "midOuterRadius", s.midOuterRadius },
			{ "endOuterRadius", s.endOuterRadius },
			{ "startAngle", s.startAngle },
			{ "endAngle", s.endAngle },
			{ "division", s.division },
			{ "isUvSwap", s.isUvSwap },
			{ "innerColor", ToJson(s.innerColor) },
			{ "outerColor", ToJson(s.outerColor) },
			{ "fadeStartAlpha", s.fadeStartAlpha },
			{ "fadeEndAlpha", s.fadeEndAlpha },
			{ "fadeRange", s.fadeRange }
		};
	} else if (auto* cs = emitter->GetCylinderShape()) {
		const auto& s = cs->settings;
		root["cylinderSettings"] = {
			{ "height", s.height },
			{ "topRadius", ToJson(s.topRadius) },
			{ "bottomRadius", ToJson(s.bottomRadius) },
			{ "startAngle", s.startAngle },
			{ "endAngle", s.endAngle },
			{ "division", s.division },
			{ "verticalDivision", s.verticalDivision },
			{ "flipV", s.flipV },
			{ "isUvSwap", s.isUvSwap },
			{ "topColor", ToJson(s.topColor) },
			{ "bottomColor", ToJson(s.bottomColor) },
			{ "fadeStartAlpha", s.fadeStartAlpha },
			{ "fadeEndAlpha", s.fadeEndAlpha },
			{ "fadeRange", s.fadeRange },
			{ "alphaReference", s.alphaReference }
		};
	} else if (auto* ps = emitter->GetPlaneShape()) {
		const auto& s = ps->settings;
		root["planeSettings"] = {
			{ "size", ToJson(s.size) },
			{ "divisionX", s.divisionX },
			{ "divisionY", s.divisionY },
			{ "flipV", s.flipV },
			{ "isUvSwap", s.isUvSwap },
			{ "color", ToJson(s.color) }
		};
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
	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		return false;
	}

	json root;
	try {
		ifs >> root;
	} catch (...) {
		return false;
	}

	std::string texPath = root.value("texture", "Particles/circle.png");
	TextureManager::GetInstance()->LoadTexture(texPath);

	// グループが未登録なら生成、既存ならテクスチャを更新
	if (ParticleManager::GetInstance()->GetEmitter(groupName) == nullptr) {
		ParticleManager::GetInstance()->CreateParticleGroup(groupName, texPath);
	} else {
		ParticleManager::GetInstance()->SetGroupTexture(groupName, texPath);
	}

	if (root.contains("blendMode")) {
		int bm = root["blendMode"].get<int>();
		if (bm >= 0 && bm < BlendMode::BlendState::kCountOfBlendMode) {
			blendMode_ = static_cast<BlendMode::BlendState>(bm);
		}
	}
	if (root.contains("useBillboard")) {
		useBillboard_ = root["useBillboard"].get<bool>();
	}

	ParticleEmitter emitter;
	if (root.contains("emitter")) {
		const auto& je = root["emitter"];
		if (je.contains("transform")) {
			const auto& jt = je["transform"];
			emitter.transform.scale = FromJsonVec3(jt.value("scale", json::array()), { 1.0f, 1.0f, 1.0f });
			emitter.transform.rotate = FromJsonVec3(jt.value("rotate", json::array()), { 0.0f, 0.0f, 0.0f });
			emitter.transform.translate = FromJsonVec3(jt.value("translate", json::array()), { 0.0f, 0.0f, 0.0f });
		}
		emitter.count = je.value("count", 1u);
		emitter.frequency = je.value("frequency", 0.1f);
		emitter.isEmit = je.value("isEmit", false);
		emitter.isEffectMode = je.value("isEffectMode", false);
		emitter.isLoop = je.value("isLoop", false);
	}

	if (root.contains("generateSettings")) {
		const auto& jg = root["generateSettings"];
		auto& gs = emitter.generateSettings;
		gs.isRandomScale = jg.value("isRandomScale", gs.isRandomScale);
		gs.scaleMin = FromJsonVec3(jg.value("scaleMin", json::array()), gs.scaleMin);
		gs.scaleMax = FromJsonVec3(jg.value("scaleMax", json::array()), gs.scaleMax);
		gs.fixedScale = FromJsonVec3(jg.value("fixedScale", json::array()), gs.fixedScale);

		gs.isRandomRotate = jg.value("isRandomRotate", gs.isRandomRotate);
		gs.rotateMin = FromJsonVec3(jg.value("rotateMin", json::array()), gs.rotateMin);
		gs.rotateMax = FromJsonVec3(jg.value("rotateMax", json::array()), gs.rotateMax);
		gs.fixedRotate = FromJsonVec3(jg.value("fixedRotate", json::array()), gs.fixedRotate);

		gs.isRandomVelocity = jg.value("isRandomVelocity", gs.isRandomVelocity);
		gs.velocityMin = FromJsonVec3(jg.value("velocityMin", json::array()), gs.velocityMin);
		gs.velocityMax = FromJsonVec3(jg.value("velocityMax", json::array()), gs.velocityMax);
		gs.fixedVelocity = FromJsonVec3(jg.value("fixedVelocity", json::array()), gs.fixedVelocity);

		gs.isRandomLifeTime = jg.value("isRandomLifeTime", gs.isRandomLifeTime);
		gs.lifeTimeMin = jg.value("lifeTimeMin", gs.lifeTimeMin);
		gs.lifeTimeMax = jg.value("lifeTimeMax", gs.lifeTimeMax);
		gs.fixedLifeTime = jg.value("fixedLifeTime", gs.fixedLifeTime);

		gs.isRandomColor = jg.value("isRandomColor", gs.isRandomColor);
		gs.colorMin = FromJsonVec4(jg.value("colorMin", json::array()), gs.colorMin);
		gs.colorMax = FromJsonVec4(jg.value("colorMax", json::array()), gs.colorMax);
		gs.fixedColor = FromJsonVec4(jg.value("fixedColor", json::array()), gs.fixedColor);
	}

	if (root.contains("fieldSettings")) {
		const auto& jf = root["fieldSettings"];
		auto& fsSettings = emitter.fieldSettings;
		fsSettings.isAccelerationFieldActive = jf.value("isAccelerationFieldActive", false);
		fsSettings.accelerationField.acceleration = FromJsonVec3(jf.value("acceleration", json::array()), fsSettings.accelerationField.acceleration);
		fsSettings.accelerationField.area.min = FromJsonVec3(jf.value("areaMin", json::array()), fsSettings.accelerationField.area.min);
		fsSettings.accelerationField.area.max = FromJsonVec3(jf.value("areaMax", json::array()), fsSettings.accelerationField.area.max);
		fsSettings.isGravityFieldActive = jf.value("isGravityFieldActive", false);
		fsSettings.gravity = FromJsonVec3(jf.value("gravity", json::array()), fsSettings.gravity);
	}

	if (root.contains("uvAnimationSettings")) {
		const auto& ju = root["uvAnimationSettings"];
		auto& uvas = emitter.uvAnimationSettings;
		uvas.isActive = ju.value("isActive", false);
		uvas.isIndividual = ju.value("isIndividual", false);
		uvas.scrollSpeed = FromJsonVec2(ju.value("scrollSpeed", json::array()), uvas.scrollSpeed);
		uvas.rotateSpeed = ju.value("rotateSpeed", 0.0f);
		uvas.scaleSpeed = FromJsonVec2(ju.value("scaleSpeed", json::array()), uvas.scaleSpeed);
	}

	int shapeTypeInt = root.value("shapeType", 0);
	emitter.SetShapeType(static_cast<ParticleShapeType>(shapeTypeInt));

	if (auto* rs = emitter.GetRingShape(); rs && root.contains("ringSettings")) {
		const auto& jr = root["ringSettings"];
		auto& s = rs->settings;
		s.innerRadius = jr.value("innerRadius", s.innerRadius);
		s.startOuterRadius = jr.value("startOuterRadius", s.startOuterRadius);
		s.midOuterRadius = jr.value("midOuterRadius", s.midOuterRadius);
		s.endOuterRadius = jr.value("endOuterRadius", s.endOuterRadius);
		s.startAngle = jr.value("startAngle", s.startAngle);
		s.endAngle = jr.value("endAngle", s.endAngle);
		s.division = jr.value("division", s.division);
		s.isUvSwap = jr.value("isUvSwap", s.isUvSwap);
		s.innerColor = FromJsonVec4(jr.value("innerColor", json::array()), s.innerColor);
		s.outerColor = FromJsonVec4(jr.value("outerColor", json::array()), s.outerColor);
		s.fadeStartAlpha = jr.value("fadeStartAlpha", s.fadeStartAlpha);
		s.fadeEndAlpha = jr.value("fadeEndAlpha", s.fadeEndAlpha);
		s.fadeRange = jr.value("fadeRange", s.fadeRange);
		rs->MarkDirty();
	} else if (auto* cs = emitter.GetCylinderShape(); cs && root.contains("cylinderSettings")) {
		const auto& jc = root["cylinderSettings"];
		auto& s = cs->settings;
		s.height = jc.value("height", s.height);
		s.topRadius = FromJsonVec2(jc.value("topRadius", json::array()), s.topRadius);
		s.bottomRadius = FromJsonVec2(jc.value("bottomRadius", json::array()), s.bottomRadius);
		s.startAngle = jc.value("startAngle", s.startAngle);
		s.endAngle = jc.value("endAngle", s.endAngle);
		s.division = jc.value("division", s.division);
		s.verticalDivision = jc.value("verticalDivision", s.verticalDivision);
		s.flipV = jc.value("flipV", s.flipV);
		s.isUvSwap = jc.value("isUvSwap", s.isUvSwap);
		s.topColor = FromJsonVec4(jc.value("topColor", json::array()), s.topColor);
		s.bottomColor = FromJsonVec4(jc.value("bottomColor", json::array()), s.bottomColor);
		s.fadeStartAlpha = jc.value("fadeStartAlpha", s.fadeStartAlpha);
		s.fadeEndAlpha = jc.value("fadeEndAlpha", s.fadeEndAlpha);
		s.fadeRange = jc.value("fadeRange", s.fadeRange);
		s.alphaReference = jc.value("alphaReference", s.alphaReference);
		cs->MarkDirty();
	} else if (auto* ps = emitter.GetPlaneShape(); ps && root.contains("planeSettings")) {
		const auto& jp = root["planeSettings"];
		auto& s = ps->settings;
		s.size = FromJsonVec2(jp.value("size", json::array()), s.size);
		s.divisionX = jp.value("divisionX", s.divisionX);
		s.divisionY = jp.value("divisionY", s.divisionY);
		s.flipV = jp.value("flipV", s.flipV);
		s.isUvSwap = jp.value("isUvSwap", s.isUvSwap);
		s.color = FromJsonVec4(jp.value("color", json::array()), s.color);
		ps->MarkDirty();
	}

	ParticleManager::GetInstance()->SetEmitter(groupName, emitter);
	return true;
}

std::string EffectEditor::GenerateCppCode(const std::string& groupName, const std::string& varPrefix) const {
	ParticleEmitter* emitter = ParticleManager::GetInstance()->GetEmitter(groupName);
	if (!emitter) {
		return "// Emitter not found: " + groupName;
	}

	std::string texPath = ParticleManager::GetInstance()->GetGroupTexture(groupName);
	std::string prefix = varPrefix.empty() ? "effect" : varPrefix;
	std::string emitterVar = prefix + "Emitter";
	std::string transformVar = prefix + "Transform";

	// C++コード生成
	std::ostringstream oss;
	oss << "// --- " << groupName << " ---\n";
	oss << "TextureManager::GetInstance()->LoadTexture(\"" << texPath << "\");\n";
	oss << "ParticleManager::GetInstance()->CreateParticleGroup(\"" << groupName << "\", \"" << texPath << "\");\n";
	oss << "{\n";
	oss << "  Transform " << transformVar << " = {\n";
	oss << "      " << FmtVec3(emitter->transform.scale) << ", // scale\n";
	oss << "      " << FmtVec3(emitter->transform.rotate) << ", // rotate\n";
	oss << "      " << FmtVec3(emitter->transform.translate) << " // translate\n";
	oss << "  };\n";
	oss << "  ParticleEmitter " << emitterVar << "(" << transformVar << ", " << emitter->count << ", " << FmtFloat(emitter->frequency) << ");\n";
	oss << "  " << emitterVar << ".isEmit = " << BoolStr(emitter->isEmit) << ";\n";
	oss << "  " << emitterVar << ".isEffectMode = " << BoolStr(emitter->isEffectMode) << ";\n";
	oss << "  " << emitterVar << ".isLoop = " << BoolStr(emitter->isLoop) << ";\n\n";

	const auto& gs = emitter->generateSettings;
	oss << "  " << emitterVar << ".generateSettings.isRandomScale = " << BoolStr(gs.isRandomScale) << ";\n";
	if (gs.isRandomScale) {
		oss << "  " << emitterVar << ".generateSettings.scaleMin = " << FmtVec3(gs.scaleMin) << ";\n";
		oss << "  " << emitterVar << ".generateSettings.scaleMax = " << FmtVec3(gs.scaleMax) << ";\n";
	} else {
		oss << "  " << emitterVar << ".generateSettings.fixedScale = " << FmtVec3(gs.fixedScale) << ";\n";
	}

	oss << "  " << emitterVar << ".generateSettings.isRandomRotate = " << BoolStr(gs.isRandomRotate) << ";\n";
	if (gs.isRandomRotate) {
		oss << "  " << emitterVar << ".generateSettings.rotateMin = " << FmtVec3(gs.rotateMin) << ";\n";
		oss << "  " << emitterVar << ".generateSettings.rotateMax = " << FmtVec3(gs.rotateMax) << ";\n";
	} else {
		oss << "  " << emitterVar << ".generateSettings.fixedRotate = " << FmtVec3(gs.fixedRotate) << ";\n";
	}

	oss << "  " << emitterVar << ".generateSettings.isRandomVelocity = " << BoolStr(gs.isRandomVelocity) << ";\n";
	if (gs.isRandomVelocity) {
		oss << "  " << emitterVar << ".generateSettings.velocityMin = " << FmtVec3(gs.velocityMin) << ";\n";
		oss << "  " << emitterVar << ".generateSettings.velocityMax = " << FmtVec3(gs.velocityMax) << ";\n";
	} else {
		oss << "  " << emitterVar << ".generateSettings.fixedVelocity = " << FmtVec3(gs.fixedVelocity) << ";\n";
	}

	oss << "  " << emitterVar << ".generateSettings.isRandomLifeTime = " << BoolStr(gs.isRandomLifeTime) << ";\n";
	if (gs.isRandomLifeTime) {
		oss << "  " << emitterVar << ".generateSettings.lifeTimeMin = " << FmtFloat(gs.lifeTimeMin) << ";\n";
		oss << "  " << emitterVar << ".generateSettings.lifeTimeMax = " << FmtFloat(gs.lifeTimeMax) << ";\n";
	} else {
		oss << "  " << emitterVar << ".generateSettings.fixedLifeTime = " << FmtFloat(gs.fixedLifeTime) << ";\n";
	}

	oss << "  " << emitterVar << ".generateSettings.isRandomColor = " << BoolStr(gs.isRandomColor) << ";\n";
	if (gs.isRandomColor) {
		oss << "  " << emitterVar << ".generateSettings.colorMin = " << FmtVec4(gs.colorMin) << ";\n";
		oss << "  " << emitterVar << ".generateSettings.colorMax = " << FmtVec4(gs.colorMax) << ";\n";
	} else {
		oss << "  " << emitterVar << ".generateSettings.fixedColor = " << FmtVec4(gs.fixedColor) << ";\n";
	}
	oss << "\n";

	const auto& fsSettings = emitter->fieldSettings;
	oss << "  " << emitterVar << ".fieldSettings.isAccelerationFieldActive = " << BoolStr(fsSettings.isAccelerationFieldActive) << ";\n";
	if (fsSettings.isAccelerationFieldActive) {
		oss << "  " << emitterVar << ".fieldSettings.accelerationField.acceleration = " << FmtVec3(fsSettings.accelerationField.acceleration) << ";\n";
		oss << "  " << emitterVar << ".fieldSettings.accelerationField.area = {" << FmtVec3(fsSettings.accelerationField.area.min) << ", " << FmtVec3(fsSettings.accelerationField.area.max) << "};\n";
	}
	oss << "  " << emitterVar << ".fieldSettings.isGravityFieldActive = " << BoolStr(fsSettings.isGravityFieldActive) << ";\n";
	if (fsSettings.isGravityFieldActive) {
		oss << "  " << emitterVar << ".fieldSettings.gravity = " << FmtVec3(fsSettings.gravity) << ";\n";
	}
	oss << "\n";

	const auto& uvas = emitter->uvAnimationSettings;
	oss << "  " << emitterVar << ".uvAnimationSettings.isActive = " << BoolStr(uvas.isActive) << ";\n";
	if (uvas.isActive) {
		oss << "  " << emitterVar << ".uvAnimationSettings.isIndividual = " << BoolStr(uvas.isIndividual) << ";\n";
		oss << "  " << emitterVar << ".uvAnimationSettings.scrollSpeed = " << FmtVec2(uvas.scrollSpeed) << ";\n";
		oss << "  " << emitterVar << ".uvAnimationSettings.rotateSpeed = " << FmtFloat(uvas.rotateSpeed) << ";\n";
		oss << "  " << emitterVar << ".uvAnimationSettings.scaleSpeed = " << FmtVec2(uvas.scaleSpeed) << ";\n";
	}
	oss << "\n";

	switch (emitter->GetShapeType()) {
	case ParticleShapeType::Billboard:
		oss << "  " << emitterVar << ".SetShapeType(ParticleShapeType::Billboard);\n";
		break;
	case ParticleShapeType::Ring:
		oss << "  " << emitterVar << ".SetShapeType(ParticleShapeType::Ring);\n";
		if (auto* rs = emitter->GetRingShape()) {
			const auto& s = rs->settings;
			oss << "  if (auto *rs = " << emitterVar << ".GetRingShape()) {\n";
			oss << "    rs->settings.innerRadius = " << FmtFloat(s.innerRadius) << ";\n";
			oss << "    rs->settings.startOuterRadius = " << FmtFloat(s.startOuterRadius) << ";\n";
			oss << "    rs->settings.midOuterRadius = " << FmtFloat(s.midOuterRadius) << ";\n";
			oss << "    rs->settings.endOuterRadius = " << FmtFloat(s.endOuterRadius) << ";\n";
			oss << "    rs->settings.startAngle = " << FmtFloat(s.startAngle) << ";\n";
			oss << "    rs->settings.endAngle = " << FmtFloat(s.endAngle) << ";\n";
			oss << "    rs->settings.division = " << s.division << ";\n";
			oss << "    rs->settings.isUvSwap = " << BoolStr(s.isUvSwap) << ";\n";
			oss << "    rs->settings.innerColor = " << FmtVec4(s.innerColor) << ";\n";
			oss << "    rs->settings.outerColor = " << FmtVec4(s.outerColor) << ";\n";
			oss << "    rs->settings.fadeStartAlpha = " << FmtFloat(s.fadeStartAlpha) << ";\n";
			oss << "    rs->settings.fadeEndAlpha = " << FmtFloat(s.fadeEndAlpha) << ";\n";
			oss << "    rs->settings.fadeRange = " << FmtFloat(s.fadeRange) << ";\n";
			oss << "  }\n";
		}
		break;
	case ParticleShapeType::Cylinder:
		oss << "  " << emitterVar << ".SetShapeType(ParticleShapeType::Cylinder);\n";
		if (auto* cs = emitter->GetCylinderShape()) {
			const auto& s = cs->settings;
			oss << "  if (auto *cs = " << emitterVar << ".GetCylinderShape()) {\n";
			oss << "    cs->settings.height = " << FmtFloat(s.height) << ";\n";
			oss << "    cs->settings.topRadius = " << FmtVec2(s.topRadius) << ";\n";
			oss << "    cs->settings.bottomRadius = " << FmtVec2(s.bottomRadius) << ";\n";
			oss << "    cs->settings.startAngle = " << FmtFloat(s.startAngle) << ";\n";
			oss << "    cs->settings.endAngle = " << FmtFloat(s.endAngle) << ";\n";
			oss << "    cs->settings.division = " << s.division << ";\n";
			oss << "    cs->settings.verticalDivision = " << s.verticalDivision << ";\n";
			oss << "    cs->settings.flipV = " << BoolStr(s.flipV) << ";\n";
			oss << "    cs->settings.isUvSwap = " << BoolStr(s.isUvSwap) << ";\n";
			oss << "    cs->settings.topColor = " << FmtVec4(s.topColor) << ";\n";
			oss << "    cs->settings.bottomColor = " << FmtVec4(s.bottomColor) << ";\n";
			oss << "    cs->settings.fadeStartAlpha = " << FmtFloat(s.fadeStartAlpha) << ";\n";
			oss << "    cs->settings.fadeEndAlpha = " << FmtFloat(s.fadeEndAlpha) << ";\n";
			oss << "    cs->settings.fadeRange = " << FmtFloat(s.fadeRange) << ";\n";
			oss << "    cs->settings.alphaReference = " << FmtFloat(s.alphaReference) << ";\n";
			oss << "  }\n";
		}
		break;
	case ParticleShapeType::Plane:
		oss << "  " << emitterVar << ".SetShapeType(ParticleShapeType::Plane);\n";
		if (auto* ps = emitter->GetPlaneShape()) {
			const auto& s = ps->settings;
			oss << "  if (auto *ps = " << emitterVar << ".GetPlaneShape()) {\n";
			oss << "    ps->settings.size = " << FmtVec2(s.size) << ";\n";
			oss << "    ps->settings.divisionX = " << s.divisionX << ";\n";
			oss << "    ps->settings.divisionY = " << s.divisionY << ";\n";
			oss << "    ps->settings.flipV = " << BoolStr(s.flipV) << ";\n";
			oss << "    ps->settings.isUvSwap = " << BoolStr(s.isUvSwap) << ";\n";
			oss << "    ps->settings.color = " << FmtVec4(s.color) << ";\n";
			oss << "  }\n";
		}
		break;
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
		DrawGroupSelectorSection();

		ParticleEmitter* emitter = selectedGroupName_.empty()
			? nullptr
			: ParticleManager::GetInstance()->GetEmitter(selectedGroupName_);

		if (emitter) {
			ImGui::Separator();
			DrawPreviewControlSection(camera, selectedGroupName_, emitter);
			ImGui::Separator();
			DrawEmitterSettingsSection(selectedGroupName_, emitter);
			DrawShapeSettingsSection(emitter);
			ImGui::Separator();
			DrawCodeExportSection(selectedGroupName_);
			ImGui::Separator();
			DrawJsonSection(selectedGroupName_);
		} else {
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
					defaultEmitter.isEmit = false;
					defaultEmitter.isEffectMode = true;
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

void EffectEditor::DrawPreviewControlSection(const Camera* camera, const std::string& groupName, ParticleEmitter* emitter) {
	if (ImGui::CollapsingHeader("Preview & Play Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Update Particles", &isUpdateParticle_);
		ImGui::SameLine();
		ImGui::Checkbox("Use Billboard", &useBillboard_);

		const char* blendNames[] = { "None", "Normal", "Add", "Subtract", "Multiply", "Screen" };
		int blendIdx = static_cast<int>(blendMode_);
		if (ImGui::Combo("Blend Mode", &blendIdx, blendNames, 6)) {
			blendMode_ = static_cast<BlendMode::BlendState>(blendIdx);
		}

		ImGui::DragFloat("Camera Front Dist", &cameraEmitDistance_, 0.1f, 0.5f, 50.0f);

		if (ImGui::Button("Emit at Emitter Pos")) {
			ParticleManager::GetInstance()->Emit(groupName, emitter->transform.translate, emitter->count);
		}
		ImGui::SameLine();
		if (ImGui::Button("Emit in Front of Camera")) {
			Vector3 pos = CalculateCameraFrontPos(camera, cameraEmitDistance_);
			ParticleManager::GetInstance()->Emit(groupName, pos, emitter->count);
		}

		if (ImGui::Button("Move Emitter to Camera Front")) {
			emitter->transform.translate = CalculateCameraFrontPos(camera, cameraEmitDistance_);
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
}

void EffectEditor::DrawEmitterSettingsSection(const std::string& groupName, ParticleEmitter* emitter) {
	auto DragFloatMinMax = [](const char* labelMin, const char* labelMax, float* minVal, float* maxVal, float speed = 0.01f) {
		if (ImGui::DragFloat(labelMin, minVal, speed, 0.0f, *maxVal)) {
			if (*minVal > *maxVal) *minVal = *maxVal;
		}
		if (ImGui::DragFloat(labelMax, maxVal, speed, *minVal, 100.0f)) {
			if (*maxVal < *minVal) *maxVal = *minVal;
		}
	};
	auto DragFloat3MinMax = [](const char* labelMin, const char* labelMax, Vector3* minVal, Vector3* maxVal, float speed = 0.01f) {
		ImGui::DragFloat3(labelMin, &minVal->x, speed);
		ImGui::DragFloat3(labelMax, &maxVal->x, speed);
		if (minVal->x > maxVal->x) minVal->x = maxVal->x;
		if (minVal->y > maxVal->y) minVal->y = maxVal->y;
		if (minVal->z > maxVal->z) minVal->z = maxVal->z;
	};

	if (ImGui::CollapsingHeader("Emitter & Animation Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("Translate", &emitter->transform.translate.x, 0.01f);
		ImGui::DragFloat3("Rotate", &emitter->transform.rotate.x, 0.01f);
		ImGui::DragFloat3("Scale", &emitter->transform.scale.x, 0.01f);

		int count = static_cast<int>(emitter->count);
		if (ImGui::DragInt("Count", &count, 1, 1, 100)) {
			emitter->count = static_cast<uint32_t>(count);
		}

		// テクスチャ選択
		std::string currentTex = ParticleManager::GetInstance()->GetGroupTexture(groupName);
		if (ImGui::BeginCombo("Texture", currentTex.c_str())) {
			for (const auto& tex : textureCandidates_) {
				bool isSelected = (currentTex == tex);
				if (ImGui::Selectable(tex.c_str(), isSelected)) {
					ParticleManager::GetInstance()->SetGroupTexture(groupName, tex);
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

		ImGui::Checkbox("Enable Auto Emit", &emitter->isEmit);
		ImGui::DragFloat("Frequency", &emitter->frequency, 0.01f, 0.01f, 10.0f);

		ImGui::Checkbox("Effect Mode (Single/Loop)", &emitter->isEffectMode);
		if (emitter->isEffectMode) {
			ImGui::Checkbox("Loop Effect", &emitter->isLoop);
			ImGui::Text("Playing State: %s", emitter->isPlaying ? "Playing" : "Stopped");
		}

		// Generate Settings
		if (ImGui::TreeNode("Generate Settings")) {
			auto& gs = emitter->generateSettings;
			ImGui::Checkbox("Random Scale", &gs.isRandomScale);
			if (gs.isRandomScale) {
				DragFloat3MinMax("Scale Min", "Scale Max", &gs.scaleMin, &gs.scaleMax);
			} else {
				ImGui::DragFloat3("Fixed Scale", &gs.fixedScale.x, 0.01f);
			}

			ImGui::Checkbox("Random Rotate", &gs.isRandomRotate);
			if (gs.isRandomRotate) {
				DragFloat3MinMax("Rotate Min", "Rotate Max", &gs.rotateMin, &gs.rotateMax);
			} else {
				ImGui::DragFloat3("Fixed Rotate", &gs.fixedRotate.x, 0.01f);
			}

			ImGui::Checkbox("Random Velocity", &gs.isRandomVelocity);
			if (gs.isRandomVelocity) {
				DragFloat3MinMax("Velocity Min", "Velocity Max", &gs.velocityMin, &gs.velocityMax);
			} else {
				ImGui::DragFloat3("Fixed Velocity", &gs.fixedVelocity.x, 0.01f);
			}

			ImGui::Checkbox("Random LifeTime", &gs.isRandomLifeTime);
			if (gs.isRandomLifeTime) {
				DragFloatMinMax("LifeTime Min", "LifeTime Max", &gs.lifeTimeMin, &gs.lifeTimeMax);
			} else {
				ImGui::DragFloat("Fixed LifeTime", &gs.fixedLifeTime, 0.01f, 0.01f, 60.0f);
			}

			ImGui::Checkbox("Random Color", &gs.isRandomColor);
			if (gs.isRandomColor) {
				ImGui::ColorEdit4("Color Min", &gs.colorMin.x);
				ImGui::ColorEdit4("Color Max", &gs.colorMax.x);
			} else {
				ImGui::ColorEdit4("Fixed Color", &gs.fixedColor.x);
			}
			ImGui::TreePop();
		}

		// Field Settings
		if (ImGui::TreeNode("Field Settings")) {
			auto& fsSettings = emitter->fieldSettings;
			ImGui::Checkbox("Acceleration Field Active", &fsSettings.isAccelerationFieldActive);
			if (fsSettings.isAccelerationFieldActive) {
				ImGui::DragFloat3("Acceleration", &fsSettings.accelerationField.acceleration.x, 0.1f);
				DragFloat3MinMax("Area Min", "Area Max", &fsSettings.accelerationField.area.min, &fsSettings.accelerationField.area.max, 0.1f);
			}
			ImGui::Separator();
			ImGui::Checkbox("Gravity Field Active", &fsSettings.isGravityFieldActive);
			if (fsSettings.isGravityFieldActive) {
				ImGui::DragFloat3("Gravity Vector", &fsSettings.gravity.x, 0.1f);
			}
			ImGui::TreePop();
		}

		// UV Animation Settings
		if (ImGui::TreeNode("UV Animation Settings")) {
			auto& uvas = emitter->uvAnimationSettings;
			ImGui::Checkbox("UV Animation Active", &uvas.isActive);
			if (uvas.isActive) {
				ImGui::Checkbox("Individual UV Animation (Effect Mode)", &uvas.isIndividual);
				ImGui::DragFloat2("Scroll Speed (U, V)", &uvas.scrollSpeed.x, 0.01f);
				ImGui::DragFloat("Rotate Speed (rad/s)", &uvas.rotateSpeed, 0.01f);
				ImGui::DragFloat2("Scale Speed (U, V)", &uvas.scaleSpeed.x, 0.01f);

				if (ImGui::Button("Reset UV States")) {
					uvas.currentTranslate = { 0.0f, 0.0f };
					uvas.currentRotate = 0.0f;
					uvas.currentScale = { 1.0f, 1.0f };
				}
			}
			ImGui::TreePop();
		}
	}
}

void EffectEditor::DrawShapeSettingsSection(ParticleEmitter* emitter) {
	auto DragFloatMinMax = [](const char* labelMin, const char* labelMax, float* minVal, float* maxVal, float speed = 0.01f) {
		if (ImGui::DragFloat(labelMin, minVal, speed, 0.0f, *maxVal)) {
			if (*minVal > *maxVal) *minVal = *maxVal;
		}
		if (ImGui::DragFloat(labelMax, maxVal, speed, *minVal, 100.0f)) {
			if (*maxVal < *minVal) *maxVal = *minVal;
		}
	};

	if (ImGui::CollapsingHeader("Shape Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* shapeItems[] = { "Billboard", "Ring", "Cylinder", "Plane" };
		int shapeIndex = static_cast<int>(emitter->GetShapeType());
		if (ImGui::Combo("Shape Type", &shapeIndex, shapeItems, 4)) {
			emitter->SetShapeType(static_cast<ParticleShapeType>(shapeIndex));
		}

		if (auto* rs = emitter->GetRingShape()) {
			if (ImGui::TreeNode("Ring Shape Settings")) {
				bool isMeshChanged = false;
				float tempInner = rs->settings.innerRadius;
				float tempOuter = rs->settings.startOuterRadius;
				DragFloatMinMax("Inner Radius", "Start Outer Radius", &tempInner, &tempOuter);
				if (tempInner != rs->settings.innerRadius || tempOuter != rs->settings.startOuterRadius) {
					rs->settings.innerRadius = tempInner;
					rs->settings.startOuterRadius = tempOuter;
					isMeshChanged = true;
				}
				if (ImGui::DragFloat("Mid Outer Radius", &rs->settings.midOuterRadius, 0.01f, 0.0f, 20.0f)) isMeshChanged = true;
				if (ImGui::DragFloat("End Outer Radius", &rs->settings.endOuterRadius, 0.01f, 0.0f, 20.0f)) isMeshChanged = true;
				if (ImGui::DragFloat("Start Angle (deg)", &rs->settings.startAngle, 1.0f) ||
					ImGui::DragFloat("End Angle (deg)", &rs->settings.endAngle, 1.0f)) isMeshChanged = true;

				int division = static_cast<int>(rs->settings.division);
				if (ImGui::DragInt("Division", &division, 1, 3, 256)) {
					rs->settings.division = static_cast<uint32_t>(division);
					isMeshChanged = true;
				}
				if (isMeshChanged) {
					rs->MarkDirty();
				}

				ImGui::Separator();
				ImGui::Checkbox("Swap UV (U <-> V)", &rs->settings.isUvSwap);
				ImGui::ColorEdit4("Inner Vertex Color", &rs->settings.innerColor.x);
				ImGui::ColorEdit4("Outer Vertex Color", &rs->settings.outerColor.x);
				ImGui::DragFloat("Fade Start Alpha", &rs->settings.fadeStartAlpha, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Fade End Alpha", &rs->settings.fadeEndAlpha, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Fade Range", &rs->settings.fadeRange, 0.01f, 0.0f, 0.5f);
				ImGui::TreePop();
			}
		} else if (auto* cs = emitter->GetCylinderShape()) {
			if (ImGui::TreeNode("Cylinder Shape Settings")) {
				bool isMeshChanged = false;
				if (ImGui::DragFloat("Height", &cs->settings.height, 0.01f, 0.0f, 50.0f)) isMeshChanged = true;
				if (ImGui::DragFloat2("Top Radius (X, Z)", &cs->settings.topRadius.x, 0.01f, 0.0f, 20.0f)) isMeshChanged = true;
				if (ImGui::DragFloat2("Bottom Radius (X, Z)", &cs->settings.bottomRadius.x, 0.01f, 0.0f, 20.0f)) isMeshChanged = true;
				if (ImGui::DragFloat("Start Angle (deg)", &cs->settings.startAngle, 1.0f) ||
					ImGui::DragFloat("End Angle (deg)", &cs->settings.endAngle, 1.0f)) isMeshChanged = true;

				int division = static_cast<int>(cs->settings.division);
				if (ImGui::DragInt("Division", &division, 1, 3, 256)) {
					cs->settings.division = static_cast<uint32_t>(division);
					isMeshChanged = true;
				}
				int verticalDivision = static_cast<int>(cs->settings.verticalDivision);
				if (ImGui::DragInt("Vertical Division", &verticalDivision, 1, 1, 256)) {
					cs->settings.verticalDivision = static_cast<uint32_t>(verticalDivision);
					isMeshChanged = true;
				}
				if (isMeshChanged) {
					cs->MarkDirty();
				}

				ImGui::Separator();
				ImGui::Checkbox("Swap UV (U <-> V)", &cs->settings.isUvSwap);
				ImGui::Checkbox("Flip V", &cs->settings.flipV);
				ImGui::ColorEdit4("Top Vertex Color", &cs->settings.topColor.x);
				ImGui::ColorEdit4("Bottom Vertex Color", &cs->settings.bottomColor.x);
				ImGui::DragFloat("Fade Start Alpha", &cs->settings.fadeStartAlpha, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Fade End Alpha", &cs->settings.fadeEndAlpha, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Fade Range", &cs->settings.fadeRange, 0.01f, 0.0f, 0.5f);
				ImGui::DragFloat("Alpha Reference (Discard)", &cs->settings.alphaReference, 0.01f, 0.0f, 1.0f);
				ImGui::TreePop();
			}
		} else if (auto* ps = emitter->GetPlaneShape()) {
			if (ImGui::TreeNode("Plane Shape Settings")) {
				bool isMeshChanged = false;
				if (ImGui::DragFloat2("Size", &ps->settings.size.x, 0.01f, 0.01f, 50.0f)) isMeshChanged = true;
				int divX = static_cast<int>(ps->settings.divisionX);
				int divY = static_cast<int>(ps->settings.divisionY);
				if (ImGui::DragInt("Division X", &divX, 1, 1, 256)) {
					ps->settings.divisionX = static_cast<uint32_t>(divX);
					isMeshChanged = true;
				}
				if (ImGui::DragInt("Division Y", &divY, 1, 1, 256)) {
					ps->settings.divisionY = static_cast<uint32_t>(divY);
					isMeshChanged = true;
				}
				if (isMeshChanged) {
					ps->MarkDirty();
				}

				ImGui::Separator();
				ImGui::Checkbox("Flip V", &ps->settings.flipV);
				ImGui::Checkbox("Swap UV (U <-> V)", &ps->settings.isUvSwap);
				ImGui::ColorEdit4("Plane Color", &ps->settings.color.x);
				ImGui::TreePop();
			}
		}
	}
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
