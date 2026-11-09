#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Math/MathTypes.h"

#include <externals/nlohmann/json.hpp>

// レベルデータ
struct LevelData {

	struct BezierControlPoint {
		Vector3 co;
		Vector3 handleLeft;
		Vector3 handleRight;
	};

	struct ObjectData {
		// 種類 (MESH, LIGHT, CAMERA, EMPTY etc...)
		std::string type;
		// オブジェクト名
		std::string name;
		// モデルファイル名 (MESHのみ)
		std::string fileName;
		std::string enemyType = "NORMAL";
		// 平行移動
		Vector3 translation;
		// 回転角 (ラジアン)
		Vector3 rotation;
		// スケーリング
		Vector3 scaling;
		// 子要素
		std::vector<ObjectData> children;
		// ベジェ制御点 (CURVEのみ)
		std::vector<BezierControlPoint> bezierPoints;
	};

	struct PlayerSpawnData {
		Vector3 translation;
		Vector3 rotation;
		float distance = 0.0f;
		int area = 0;
	};

	struct EnemySpawnData {
		std::string fileName;
		std::string enemyType = "NORMAL";
		Vector3 translation;
		Vector3 rotation;
		float distance = 0.0f;
		int area = 0;
	};

	// ルートオブジェクトの配列
	std::vector<ObjectData> objects;
	std::vector<PlayerSpawnData> players;
	std::vector<EnemySpawnData> enemies;
};

/// <summary>
/// Blenderから出力されたJSONレベルデータを読み込むクラス
/// </summary>
class LevelLoader {

public:// 定数
	// デフォルトの読み込みディレクトリ
	static const std::string kDefaultBaseDirectory;
	// ファイル拡張子
	static const std::string kExtension;

public:// メンバ関数
	
	/// <summary>
	/// レベルデータファイルの読み込み
	/// </summary>
	/// <param name="fileName">ファイル名 (拡張子なし)</param>
	/// <returns>読み込まれたレベルデータ</returns>
	static std::unique_ptr<LevelData> LoadFile(const std::string& fileName);

private:
	/// <summary>
	/// オブジェクトとその子要素を再帰的にパースする
	/// </summary>
	/// <param name="objectData">格納先の参照</param>
	/// <param name="jsonObject">パース対象のJSONオブジェクト</param>
	static void ParseObject(LevelData::ObjectData& objectData, const nlohmann::json& jsonObject);
};
