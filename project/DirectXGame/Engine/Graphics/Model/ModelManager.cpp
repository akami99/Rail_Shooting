#include "ModelManager.h"
#include "Model.h"

#include "../../Core/Utility/Logger/Logger.h"
#include "../../Core/Utility/String/StringUtility.h"

#include <cassert>
#include <filesystem>
#include <format>
#include <utility>

using namespace Logger;
using namespace StringUtility;

std::unique_ptr<ModelManager> ModelManager::instance_ = nullptr;

// 初期化
void ModelManager::Initialize() {
}

// シングルトンインスタンスの取得
ModelManager *ModelManager::GetInstance() {
  if (instance_ == nullptr) {
    instance_ = std::make_unique<ModelManager>(Token{});
  }
  return instance_.get();
}

void ModelManager::Destroy() {
    instance_.reset();
}

ModelManager::ModelManager(Token) {
    // コンストラクタ
}

// 終了
void ModelManager::Finalize() {
  instance_.reset();
}

Model *ModelManager::FindModel(const std::string &filePath) {
  // --- 読み込み済みモデルを検索し、存在すればreturn ---
  if (models_.contains(filePath)) {
    // 読み込み済みなのでreturn
    return models_.at(filePath).get();
  }
  // ファイル名一致無し
  return nullptr;
}

void ModelManager::LoadModel(const std::string &directoryPath,
                             const std::string &filePath) {
    std::string fullDirectoryPath = directoryPath;

    // directoryPath 自体が "Resources/" を含んでいない場合のみ、ベースパスを付与する
    if (directoryPath.find("Resources/") == std::string::npos &&
        directoryPath.find("resources/") == std::string::npos) {
        fullDirectoryPath = "Resources/Assets/Models/" + directoryPath;
    }

    // ファイルのフルパスを作成
    std::string fullFilePath = fullDirectoryPath + "/" + filePath;

    // 存在チェック
    if (!std::filesystem::exists(fullFilePath)) {
        Logger::Log("ERROR: Model file not found at: " + fullFilePath);
        assert(false && "Model file not found!");
    }

  // --- 読み込み済みモデルを検索し、存在すれば早期return ---
  if (models_.contains(filePath)) {
    // 読み込み済みなので何もしない
    return;
  }

  // --- モデルの生成とファイル読み込み、初期化 ---
  // unique_ptrでModelのインスタンスを生成
  std::unique_ptr<Model> model = std::make_unique<Model>();

  model->Initialize(fullDirectoryPath, filePath);
  Log("INFO: Loaded model at path: " + fullFilePath + "\n");

  // --- 4. モデルをマップコンテナに格納 ---
  // std::unique_ptr はコピー禁止なので、std::move() によって所有権を移譲する
  models_.insert(std::make_pair(filePath, std::move(model)));
}

void ModelManager::CreateRing(const std::string& name, const std::string& textureFilePath, float innerRadius, float outerRadius, uint32_t division) {
    // すでに存在すれば早期リターン
    if (models_.contains(name)) {
        return;
    }

    // モデルの生成
    std::unique_ptr<Model> model = std::make_unique<Model>();
    model->CreateRing(textureFilePath, innerRadius, outerRadius, division);

    // マップに格納
    models_.insert(std::make_pair(name, std::move(model)));
    Log(std::format("INFO: Created ring primitive: {}\n", name));
}
