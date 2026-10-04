#include "LevelLoader.h"
#include <fstream>
#include <cassert>

// ディレクトリ定数の初期化
const std::string LevelLoader::kDefaultBaseDirectory = "Resources/levels/";
const std::string LevelLoader::kExtension = ".json";

std::unique_ptr<LevelData> LevelLoader::LoadFile(const std::string& fileName) {
	// フルパスの構築
	const std::string fullPath = kDefaultBaseDirectory + fileName + kExtension;

	// ファイルを開く
	std::ifstream file;
	file.open(fullPath);
	if (file.fail()) {
		// ファイルが開けない場合はエラーを出して中断
		assert(0 && "Failed to open level file.");
		return nullptr;
	}

	// JSONデータをパース
	nlohmann::json deserialized;
	file >> deserialized;

	// 正しいシーンデータかチェック
	assert(deserialized.is_object());
	assert(deserialized.contains("name"));
	assert(deserialized["name"].get<std::string>() == "scene");

	// レベルデータコンテナを生成
	auto levelData = std::make_unique<LevelData>();

	// 全オブジェクトを走査
	for (const auto& object : deserialized["objects"]) {
		assert(object.contains("type"));

		if (object.contains("disabled")) {
			bool disabled = object["disabled"].get<bool>();
			if (disabled) {
				continue;
			}
		}

		if (object.contains("spawn")) {
			const std::string spawnType = object["spawn"].get<std::string>();
			if (spawnType == "PLAYER") {
				levelData->players.emplace_back(LevelData::PlayerSpawnData{});
				LevelData::ObjectData temp{};
				ParseObject(temp, object);
				levelData->players.back().translation = temp.translation;
				levelData->players.back().rotation = temp.rotation;
				if (object.contains("distance")) {
					levelData->players.back().distance = object["distance"].get<float>();
				}
				if (object.contains("area")) {
					levelData->players.back().area = object["area"].get<int>();
				}
				continue;
			}
			else if (spawnType == "ENEMY") {
				levelData->enemies.emplace_back(LevelData::EnemySpawnData{});
				LevelData::ObjectData temp{};
				ParseObject(temp, object);
				levelData->enemies.back().translation = temp.translation;
				levelData->enemies.back().rotation = temp.rotation;
				if (object.contains("file_name")) {
					levelData->enemies.back().fileName = object["file_name"].get<std::string>();
				}
				if (object.contains("distance")) {
					levelData->enemies.back().distance = object["distance"].get<float>();
				}
				if (object.contains("area")) {
					levelData->enemies.back().area = object["area"].get<int>();
				}
				continue;
			}
		}

		levelData->objects.emplace_back(LevelData::ObjectData{});
		ParseObject(levelData->objects.back(), object);
	}

	return levelData;
}

void LevelLoader::ParseObject(LevelData::ObjectData& objectData, const nlohmann::json& jsonObject) {
	// 基本情報のパース
	objectData.type = jsonObject["type"].get<std::string>();
	objectData.name = jsonObject["name"].get<std::string>();

	// MESHタイプ等のファイル名
	if (jsonObject.contains("file_name")) {
		objectData.fileName = jsonObject["file_name"].get<std::string>();
	}

	// トランスフォームの取得
	const auto& transform = jsonObject["transform"];

	// --- 座標系の変換 (Blender -> Engine) ---
	// Blender: 右手系, Z-Up
	// Engine:  左手系, Y-Up (DirectX標準)
	
	// Translation (X -> -X, Z -> Y, Y -> -Z)
	objectData.translation.x = -static_cast<float>(transform["translation"][0]);
	objectData.translation.y = static_cast<float>(transform["translation"][2]);
	objectData.translation.z = -static_cast<float>(transform["translation"][1]);

	// Rotation (度数法からラジアンへ変換し、符号を反転)
	// Blenderの回転軸(X,Y,Z)をEngineの回転軸(X,Z,Y)に変換し、右手系から左手系への変換のため符号を調整
	float toRad = 3.1415926535f / 180.0f;
	objectData.rotation.x = -(static_cast<float>(transform["rotation"][0]) * toRad);
	objectData.rotation.y = -(static_cast<float>(transform["rotation"][2]) * toRad);
	objectData.rotation.z = (static_cast<float>(transform["rotation"][1]) * toRad);

	// Scaling (軸を入れ替え)
	objectData.scaling.x = static_cast<float>(transform["scaling"][0]);
	objectData.scaling.y = static_cast<float>(transform["scaling"][2]);
	objectData.scaling.z = static_cast<float>(transform["scaling"][1]);

	// --- ベジェ制御点のパース ---
	if (jsonObject.contains("curve")) {
		const auto& curve = jsonObject["curve"];
		if (curve.contains("splines")) {
			for (const auto& spline : curve["splines"]) {
				if (spline.contains("bezier_points")) {
					for (const auto& pt : spline["bezier_points"]) {
						LevelData::BezierControlPoint controlPt;
						
						const auto& co = pt["co"];
						controlPt.co.x = -static_cast<float>(co[0]);
						controlPt.co.y = static_cast<float>(co[2]);
						controlPt.co.z = -static_cast<float>(co[1]);

						const auto& hl = pt["handle_left"];
						controlPt.handleLeft.x = -static_cast<float>(hl[0]);
						controlPt.handleLeft.y = static_cast<float>(hl[2]);
						controlPt.handleLeft.z = -static_cast<float>(hl[1]);

						const auto& hr = pt["handle_right"];
						controlPt.handleRight.x = -static_cast<float>(hr[0]);
						controlPt.handleRight.y = static_cast<float>(hr[2]);
						controlPt.handleRight.z = -static_cast<float>(hr[1]);

						objectData.bezierPoints.push_back(controlPt);
					}
				}
			}
		}
	}

	// --- 子要素の再帰パース ---
	if (jsonObject.contains("children")) {
		for (const auto& child : jsonObject["children"]) {
			objectData.children.emplace_back(LevelData::ObjectData{});
			ParseObject(objectData.children.back(), child);
		}
	}
}
