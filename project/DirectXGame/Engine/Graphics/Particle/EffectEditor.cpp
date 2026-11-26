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
	if (const auto* rs = emitter->GetRingShape()) {
		const auto& s = rs->GetSettings();
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
	} else if (const auto* cs = emitter->GetCylinderShape()) {
		const auto& s = cs->GetSettings();
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
	} else if (const auto* ps = emitter->GetPlaneShape()) {
		const auto& s = ps->GetSettings();
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
			Transform t;
			t.scale = FromJsonVec3(jt.value("scale", json::array()), { 1.0f, 1.0f, 1.0f });
			t.rotate = FromJsonVec3(jt.value("rotate", json::array()), { 0.0f, 0.0f, 0.0f });
			t.translate = FromJsonVec3(jt.value("translate", json::array()), { 0.0f, 0.0f, 0.0f });
			emitter.SetTransform(t);
		}
		emitter.SetCount(je.value("count", 1u));
		emitter.SetFrequency(je.value("frequency", 0.1f));
		emitter.SetIsEmit(je.value("isEmit", false));
		emitter.SetIsEffectMode(je.value("isEffectMode", false));
		emitter.SetIsLoop(je.value("isLoop", false));
	}

	if (root.contains("generateSettings")) {
		const auto& jg = root["generateSettings"];
		ParticleGenerateSettings gs = emitter.GetGenerateSettings();
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
		emitter.SetGenerateSettings(gs);
	}

	if (root.contains("fieldSettings")) {
		const auto& jf = root["fieldSettings"];
		ParticleFieldSettings fsSettings = emitter.GetFieldSettings();
		fsSettings.isAccelerationFieldActive = jf.value("isAccelerationFieldActive", false);
		fsSettings.accelerationField.acceleration = FromJsonVec3(jf.value("acceleration", json::array()), fsSettings.accelerationField.acceleration);
		fsSettings.accelerationField.area.min = FromJsonVec3(jf.value("areaMin", json::array()), fsSettings.accelerationField.area.min);
		fsSettings.accelerationField.area.max = FromJsonVec3(jf.value("areaMax", json::array()), fsSettings.accelerationField.area.max);
		fsSettings.isGravityFieldActive = jf.value("isGravityFieldActive", false);
		fsSettings.gravity = FromJsonVec3(jf.value("gravity", json::array()), fsSettings.gravity);
		emitter.SetFieldSettings(fsSettings);
	}

	if (root.contains("uvAnimationSettings")) {
		const auto& ju = root["uvAnimationSettings"];
		ParticleUVAnimationSettings uvas = emitter.GetUVAnimationSettings();
		uvas.isActive = ju.value("isActive", false);
		uvas.isIndividual = ju.value("isIndividual", false);
		uvas.scrollSpeed = FromJsonVec2(ju.value("scrollSpeed", json::array()), uvas.scrollSpeed);
		uvas.rotateSpeed = ju.value("rotateSpeed", 0.0f);
		uvas.scaleSpeed = FromJsonVec2(ju.value("scaleSpeed", json::array()), uvas.scaleSpeed);
		emitter.SetUVAnimationSettings(uvas);
	}

	int shapeTypeInt = root.value("shapeType", 0);
	emitter.SetShapeType(static_cast<ParticleShapeType>(shapeTypeInt));

	if (auto* rs = emitter.GetRingShape(); rs && root.contains("ringSettings")) {
		const auto& jr = root["ringSettings"];
		RingSettings s = rs->GetSettings();
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
		rs->SetSettings(s);
	} else if (auto* cs = emitter.GetCylinderShape(); cs && root.contains("cylinderSettings")) {
		const auto& jc = root["cylinderSettings"];
		CylinderSettings s = cs->GetSettings();
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
		cs->SetSettings(s);
	} else if (auto* ps = emitter.GetPlaneShape(); ps && root.contains("planeSettings")) {
		const auto& jp = root["planeSettings"];
		PlaneSettings s = ps->GetSettings();
		s.size = FromJsonVec2(jp.value("size", json::array()), s.size);
		s.divisionX = jp.value("divisionX", s.divisionX);
		s.divisionY = jp.value("divisionY", s.divisionY);
		s.flipV = jp.value("flipV", s.flipV);
		s.isUvSwap = jp.value("isUvSwap", s.isUvSwap);
		s.color = FromJsonVec4(jp.value("color", json::array()), s.color);
		ps->SetSettings(s);
	}

	ParticleManager::GetInstance()->SetEmitter(groupName, emitter);
	return true;
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

	switch (emitter->GetShapeType()) {
	case ParticleShapeType::Billboard:
		oss << "  " << emitterVar << ".SetShapeType(ParticleShapeType::Billboard);\n";
		break;
	case ParticleShapeType::Ring:
		oss << "  " << emitterVar << ".SetShapeType(ParticleShapeType::Ring);\n";
		if (const auto* rs = emitter->GetRingShape()) {
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
		break;
	case ParticleShapeType::Cylinder:
		oss << "  " << emitterVar << ".SetShapeType(ParticleShapeType::Cylinder);\n";
		if (const auto* cs = emitter->GetCylinderShape()) {
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
		break;
	case ParticleShapeType::Plane:
		oss << "  " << emitterVar << ".SetShapeType(ParticleShapeType::Plane);\n";
		if (const auto* ps = emitter->GetPlaneShape()) {
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
			ParticleManager::GetInstance()->Emit(groupName, emitter->GetTranslate(), emitter->GetCount());
		}
		ImGui::SameLine();
		if (ImGui::Button("Emit in Front of Camera")) {
			Vector3 pos = CalculateCameraFrontPos(camera, cameraEmitDistance_);
			ParticleManager::GetInstance()->Emit(groupName, pos, emitter->GetCount());
		}

		if (ImGui::Button("Move Emitter to Camera Front")) {
			emitter->SetTranslate(CalculateCameraFrontPos(camera, cameraEmitDistance_));
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
		Vector3 translate = emitter->GetTranslate();
		if (ImGui::DragFloat3("Translate", &translate.x, 0.01f)) { emitter->SetTranslate(translate); }
		Vector3 rotate = emitter->GetRotate();
		if (ImGui::DragFloat3("Rotate", &rotate.x, 0.01f)) { emitter->SetRotate(rotate); }
		Vector3 scale = emitter->GetScale();
		if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) { emitter->SetScale(scale); }

		int count = static_cast<int>(emitter->GetCount());
		if (ImGui::DragInt("Count", &count, 1, 1, 100)) {
			emitter->SetCount(static_cast<uint32_t>(count));
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

		bool isEmit = emitter->IsEmit();
		if (ImGui::Checkbox("Enable Auto Emit", &isEmit)) { emitter->SetIsEmit(isEmit); }
		float frequency = emitter->GetFrequency();
		if (ImGui::DragFloat("Frequency", &frequency, 0.01f, 0.01f, 10.0f)) { emitter->SetFrequency(frequency); }

		bool isEffectMode = emitter->IsEffectMode();
		if (ImGui::Checkbox("Effect Mode (Single/Loop)", &isEffectMode)) { emitter->SetIsEffectMode(isEffectMode); }
		if (emitter->IsEffectMode()) {
			bool isLoop = emitter->IsLoop();
			if (ImGui::Checkbox("Loop Effect", &isLoop)) { emitter->SetIsLoop(isLoop); }
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
				RingSettings s = rs->GetSettings();
				bool changed = false;
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
		} else if (auto* cs = emitter->GetCylinderShape()) {
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
		} else if (auto* ps = emitter->GetPlaneShape()) {
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
